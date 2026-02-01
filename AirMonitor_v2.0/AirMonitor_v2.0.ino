/*
  Air Quality Monitor
  GitHub: https://github.com/fobaty/Air-Quality-Monitor
  Version: 2.0
  Glory to Ukraine!
*/
const String VERSION = "V2.0";
#include "Global.h"

// Initialize Global Objects
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_SCD30 scd30;
HardwareSerial PMS(1);
Preferences preferences;
SPIClass spiTFT(FSPI);
Adafruit_ST7735 tft = Adafruit_ST7735(&spiTFT, TFT_CS, TFT_DC, TFT_RST);

// Variables
String m_srv, m_user, m_pass;
int m_port;
bool m_en = false;
long gmtOffset_sec = 0;
int daylightOffset_sec = 0;
bool scd30Detected = false;
float co2 = 0, temperature = 0, humidity = 0;
uint16_t pm1 = 0, pm25 = 0, pm10 = 0;
unsigned long lastDisplayUpdate = 0, lastMqtt = 0, lastMqttRetry = 0, lastGraphPoint = 0, lastClockUpdate = 0;
bool lastWasTime = false;
int co2History[GRAPH_SAMPLES];
int graphIdx = 0;
unsigned long graphInterval = 30000;

void setup() {
  setCpuFrequencyMhz(160);
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  spiTFT.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_GREENTAB); tft.setRotation(0); tft.fillScreen(ST77XX_BLACK);

  // Splash Screen
  tft.drawRoundRect(5, 5, tft.width() - 10, 45, 8, ST77XX_CYAN);
  tft.setTextSize(2); tft.setCursor(20, 15); tft.setTextColor(ST77XX_CYAN); tft.print("AIR SCAN");
  tft.setTextSize(1); tft.setCursor(45, 35); tft.print(VERSION);
  tft.setCursor(10, 70); tft.setTextColor(ST77XX_WHITE); tft.println("SYSTEM CHECK:");

  auto pSt = [&](const char* m, bool ok) {
    tft.setCursor(10, tft.getCursorY()); tft.print(" > "); tft.print(m); tft.print(":");
    tft.setCursor(95, tft.getCursorY());
    if (ok) { tft.setTextColor(ST77XX_GREEN); tft.println("OK"); }
    else { tft.setTextColor(ST77XX_RED); tft.println("FAIL"); }
    tft.setTextColor(ST77XX_WHITE); delay(400);
  };

  Wire.begin(I2C_SDA, I2C_SCL);
  scd30Detected = scd30.begin();
  pSt("SCD30", scd30Detected);
  PMS.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  pSt("PMS5003", true);

  preferences.begin("mqtt-conf", true);
  m_en = preferences.getBool("m_en", false);
  m_srv = preferences.getString("m_srv", "");
  m_port = preferences.getInt("m_port", 1883);
  m_user = preferences.getString("m_user", "");
  m_pass = preferences.getString("m_pass", "");
  gmtOffset_sec = preferences.getLong("gmt_off", 0);
  daylightOffset_sec = preferences.getInt("dst_off", 0);
  preferences.end();

  tft.setCursor(10, tft.getCursorY()); tft.print(" > WiFi: ");
  if (connectToStoredWiFi()) {
    tft.setTextColor(ST77XX_GREEN); tft.println("\nCONNECTED");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (m_en && m_srv != "") mqttClient.setServer(m_srv.c_str(), m_port);
  } else {
    tft.setTextColor(ST77XX_ORANGE); tft.println("\nAP MODE ACTIVE");
    WiFi.mode(WIFI_AP_STA); WiFi.softAP(AP_SSID_DEF, AP_PASS_DEF);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
  setupWebRoutes();
  server.begin();

  delay(1000); tft.fillScreen(ST77XX_BLACK);
  lastDisplayUpdate = millis();
}

void loop() {
  unsigned long ms = millis();

  // 1. UI - Progress Bar
  drawProgressBar(ms);

  // 2. Systems
  server.handleClient();
  if (scd30Detected && scd30.dataReady()) {
    scd30.read(); co2 = scd30.CO2; temperature = scd30.temperature; humidity = scd30.relative_humidity;
  }
  readPMS();

  // 3. MQTT
  if (WiFi.status() == WL_CONNECTED && m_en) {
    reconnectMqtt(); mqttClient.loop();
    if (mqttClient.connected() && ms - lastMqtt >= 10000) {
      lastMqtt = ms;
      String p = "{\"co2\":" + String(co2, 0) + ",\"pm1\":" + String(pm1) + ",\"pm25\":" + String(pm25) + ",\"pm10\":" + String(pm10) + ",\"t\":" + String(temperature, 2) + ",\"h\":" + String(humidity, 0) + "}";
      mqttClient.publish("air/status", p.c_str());
    }
  }

  // 4. Graph Logic
  if (ms - lastGraphPoint >= graphInterval) {
    lastGraphPoint = ms;
    if (graphIdx < GRAPH_SAMPLES) co2History[graphIdx++] = (int)co2;
    else {
      for (int i = 0; i < GRAPH_SAMPLES - 1; i++) co2History[i] = co2History[i + 1];
      co2History[GRAPH_SAMPLES - 1] = (int)co2;
    }
  }

  // 5. UI - Clock
  updateClock(ms);

  // 6. UI - Refresh Data (5 sec)
  if (ms - lastDisplayUpdate >= 5000) {
    lastDisplayUpdate = ms;
    int y = 32; tft.setTextSize(1);
    uint16_t cC = (co2 <= 800) ? ST77XX_GREEN : (co2 <= 1200) ? ST77XX_ORANGE : ST77XX_RED;
    tft.setTextColor(cC, ST77XX_BLACK); tft.setCursor(1, y);
    tft.printf("CO2: %.0f (%s) ", co2, co2Level(co2).c_str());
    y += 12; drawCO2Graph(4, y, 120, 30); y += 35;
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); tft.setCursor(0, y);
    tft.printf("T:%.1fC H:%.0f%% ", temperature, humidity); y += 15;
    auto dP = [&](const char* lbl, uint16_t v, int& yp) {
      String l = pmLevel(v); uint16_t c = (l == "Good") ? ST77XX_GREEN : (l == "Fair") ? ST77XX_ORANGE : ST77XX_RED;
      tft.setTextColor(c, ST77XX_BLACK); tft.setCursor(1, yp);
      tft.printf("%s:%u ", lbl, v); tft.setCursor(70, yp); tft.print(l + " "); yp += 14;
    };
    dP("PM1.0", pm1, y); dP("PM2.5", pm25, y); dP("PM10 ", pm10, y);
    tft.fillRect(0, tft.height() - 14, tft.width(), 12, ST77XX_BLACK);
    tft.setCursor(1, tft.height() - 12);
    if (WiFi.status() == WL_CONNECTED) {
      tft.setTextColor(m_en && mqttClient.connected() ? ST77XX_GREEN : (m_en ? ST77XX_RED : ST77XX_YELLOW));
      tft.print(m_en ? (mqttClient.connected() ? "M:OK" : "M:ERR") : "M:OFF");
      tft.setTextColor(0x7BEF); tft.print(" "); tft.print(WiFi.localIP().toString());
    } else { tft.setTextColor(ST77XX_ORANGE); tft.print("AP: 192.168.4.1"); }
  }
}