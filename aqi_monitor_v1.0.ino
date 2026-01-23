/*
  Air Quality Monitor
  GitHub: https://github.com/fobaty/Air-Quality-Monitor
  Version: 1.0
  Glory to Ukraine!
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_SCD30.h>
#include <Preferences.h>
#include <PubSubClient.h> 
#include "time.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

/* ================= CONFIG ================= */
const char* AP_SSID_DEF = "AIR-SCAN-CONFIG";
const char* AP_PASS_DEF = "12345678";
const int WIFI_TIMEOUT = 12000;
const int MAX_WIFI_NETWORKS = 5;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -28800;    //time zone GMT-8
const int   daylightOffset_sec = 3600; 

#define I2C_SDA 12
#define I2C_SCL 13
#define PMS_RX 11
#define PMS_TX 10
#define TFT_SCK  1
#define TFT_MOSI 2
#define TFT_RST  3
#define TFT_DC   4
#define TFT_CS   5
#define TFT_BL   6

/* ================= GLOBALS ================= */
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_SCD30 scd30;
HardwareSerial PMS(1);
Preferences preferences;

SPIClass spiTFT(FSPI);
Adafruit_ST7735 tft = Adafruit_ST7735(&spiTFT, TFT_CS, TFT_DC, TFT_RST);

String m_srv, m_user, m_pass;
int m_port;
bool m_en = false;

bool scd30Detected = false;
float co2 = 0, temperature = 0, humidity = 0;
uint16_t pm1 = 0, pm25 = 0, pm10 = 0;
unsigned long lastDisplayUpdate = 0; 
unsigned long lastMqtt = 0, lastMqttRetry = 0, lastGraphPoint = 0, lastClockUpdate = 0;

#define GRAPH_SAMPLES 60 
int co2History[GRAPH_SAMPLES];
int graphIdx = 0;
unsigned long graphInterval = 30000;

/* ===== HELPERS ===== */
String pmLevel(uint16_t v) {
  if (v <= 12) return "Good";
  if (v <= 35) return "Fair";
  if (v <= 55) return "Poor";
  return "Bad";
}
String co2Level(float v) {
  if (v <= 800) return "Good";
  if (v <= 1200) return "Fair";
  if (v <= 2000) return "Poor";
  return "Bad";
}
String levelColor(String lvl) {
  if (lvl == "Good") return "green";
  if (lvl == "Fair") return "orange";
  if (lvl == "Poor" || lvl == "Bad") return "red";
  return "darkred";
}

/* ================= MULTI-WIFI LOGIC ================= */
void saveWiFi(String ssid, String pass) {
  if (ssid == "") return;
  preferences.begin("wifi-list", false);
  
  if (preferences.getString("s0", "") == ssid) {
    preferences.putString("p0", pass);
    preferences.end();
    return;
  }

  for (int i = MAX_WIFI_NETWORKS - 1; i > 0; i--) {
    String s = preferences.getString(("s" + String(i-1)).c_str(), "");
    String p = preferences.getString(("p" + String(i-1)).c_str(), "");
    if (s != "") {
      preferences.putString(("s" + String(i)).c_str(), s);
      preferences.putString(("p" + String(i)).c_str(), p);
    }
  }
  
  preferences.putString("s0", ssid);
  preferences.putString("p0", pass);
  preferences.end();
}

bool connectToStoredWiFi() {
  preferences.begin("wifi-list", true);
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    String s = preferences.getString(("s" + String(i)).c_str(), "");
    String p = preferences.getString(("p" + String(i)).c_str(), "");
    
    if (s == "" || s.length() < 1) continue;

    tft.printf("\nTrying [%d]: %s", i+1, s.c_str());
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    delay(500); 
    
    WiFi.begin(s.c_str(), p.c_str());
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
      delay(500); tft.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      preferences.end();
      saveWiFi(s, p); 
      return true;
    }
  }
  preferences.end();
  return false;
}

/* ================= WEB UI ================= */
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Air Monitor</title><style>
 body { font-family: sans-serif; text-align: center; background: #eceff1; margin: 0; padding: 10px; }
 .panel { background: white; padding: 20px; border-radius: 16px; display: inline-block; width: 100%; max-width: 400px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); margin-top:10px; }
 .box { padding:12px; margin:8px 0; border-radius:8px; color:white; font-weight: bold; text-align: left; }
 .view { display: none; } .active { display: block; }
 .btn { padding: 12px 20px; background: #607d8b; color: white; border: none; border-radius: 8px; cursor: pointer; width: 100%; margin-top: 10px; font-weight: bold; }
 #scan-res { text-align: left; margin: 10px 0; max-height: 150px; overflow-y: auto; background: #f9f9f9; border-radius: 8px; }
 .net-item { cursor: pointer; padding: 10px; border-bottom: 1px solid #eee; font-size: 14px; }
 .data-row { display: flex; justify-content: space-around; font-size: 1.2em; margin: 15px 0; }
 input { width: 90%; padding: 12px; margin: 8px 0; border: 1px solid #ccc; border-radius: 6px; }
 label { display: block; text-align: left; margin-top: 10px; font-weight: bold; color: #455a64; }
 .btn-danger { background: #90a4ae; font-size: 11px; padding: 5px; margin-top: 5px; width: auto; display: inline-block; } /* Сделана маленькой и серой */
 .flex-row { display: flex; gap: 10px; align-items: center; }
</style></head><body>
<div id="main-view" class="panel view active">
 <h2>Air Quality</h2><p id="st-i" style="color:#888;font-size:12px">Loading...</p>
 <div id="c-b" class="box">CO2: --</div>
 <div id="p1-b" class="box">PM1.0: --</div>
 <div id="p25-b" class="box">PM2.5: --</div>
 <div id="p10-b" class="box">PM10: --</div>
 <div class="data-row"><div>🌡️ <span id="t-v">--</span>°C</div><div>💧 <span id="h-v">--</span>%</div></div>
 <button class="btn" onclick="sh('settings-view')">⚙️ Settings</button>
</div>
<div id="settings-view" class="panel view">
 <h2>Settings</h2>
 <div style="border-bottom: 1px solid #eee; padding-bottom: 15px; margin-bottom: 15px;">
  <button class="btn" style="background:#2196f3; margin-bottom:5px;" onclick="scan()">📡 Scan WiFi</button>
  <button class="btn btn-danger" onclick="if(confirm('Erase all saved WiFi networks?')) { location.href='/clearwifi'; }">🗑️ Reset WiFi List</button>
 </div>
 
 <div id="scan-res"></div>
 <form action="/connect" method="POST">
  <label>WiFi SSID:</label><input name="ssid" id="ssid" placeholder="New Network SSID">
  <label>WiFi Password:</label><input name="pass" type="password">
  <hr>
  <label><input type="checkbox" name="m_en" id="m_en" style="width:auto"> Enable MQTT</label>
  <label>Broker IP:</label><input name="m_srv" id="m_srv" placeholder="Current: Saved">
  <label>Port:</label><input name="m_port" id="m_port" type="number" placeholder="1883">
  <label>User:</label><input name="m_user" id="m_user" placeholder="Keep current">
  <label>Pass:</label><input name="m_pass" id="m_pass" type="password" placeholder="Keep current">
  <button type="submit" class="btn" style="background:#4caf50">Save & Restart</button>
 </form>
 <button class="btn" style="background:none;color:#888" onclick="sh('main-view')">← Back</button>
</div>
<script>
function sh(id){document.querySelectorAll('.view').forEach(v=>v.classList.remove('active'));document.getElementById(id).classList.add('active');}
function update(){
 fetch("/status").then(r=>r.json()).then(d=>{
  document.getElementById("st-i").innerText = d.ssid + " | " + d.ip + " | MQTT: " + d.mq;
  uB("c-b", d.co2, d.co2lvl, d.co2clr, "CO2"); uB("p1-b", d.pm1, d.pm1lvl, d.pm1clr, "PM1.0");
  uB("p25-b", d.pm25, d.pm25lvl, d.pm25clr, "PM2.5"); uB("p10-b", d.pm10, d.pm10lvl, d.pm10clr, "PM10");
  document.getElementById("t-v").innerText = (d.temp != null)? d.temp : "--"; 
  document.getElementById("h-v").innerText = (d.hum != null)? d.hum : "--";
  if(!window.f){ document.getElementById("m_en").checked = d.m_en; window.f=true; }
 });
}
function uB(id,v,l,c,n){let e=document.getElementById(id);e.style.background=c;e.innerText=n+": "+v+" ("+l+")";}
function scan(){let r=document.getElementById("scan-res");r.innerHTML="Scanning...";fetch("/scan").then(r=>r.json()).then(data=>{r.innerHTML="";data.forEach(n=>{let d=document.createElement("div");d.className="net-item";d.innerText=n.ssid+" ("+n.rssi+"dBm)";d.onclick=()=>{document.getElementById("ssid").value=n.ssid;};r.appendChild(d);});});}
setInterval(update, 5000); update();
</script></body></html>
)rawliteral";

/* ================= FUNCTIONS ================= */

void reconnectMqtt() {
  if (!m_en || WiFi.status() != WL_CONNECTED || m_srv == "") return;
  if (mqttClient.connected()) return;
  if (millis() - lastMqttRetry > 15000) {
    lastMqttRetry = millis();
    String clientId = "AirScan-" + String(random(0xffff), HEX);
    mqttClient.connect(clientId.c_str(), m_user.c_str(), m_pass.c_str());
  }
}

void readPMS() {
  while (PMS.available() >= 32) {
    if (PMS.read() == 0x42 && PMS.read() == 0x4D) {
      uint8_t buf[30]; PMS.readBytes(buf, 30);
      pm1 = buf[8]<<8|buf[9]; pm25 = buf[10]<<8|buf[11]; pm10 = buf[12]<<8|buf[13];
    }
  }
}

void drawCO2Graph(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, ST77XX_BLACK); tft.drawRect(x, y, w, h, 0x4208);      
  int nl = map(constrain(co2, 400, 2000), 400, 2000, 0, h - 4);
  for(int i=0; i<w; i+=4) tft.drawPixel(x+i, y + h - 2 - nl, 0x03E0); 
  for (int i=0; i<GRAPH_SAMPLES-1; i++) {
    if (co2History[i+1] == 0) break;
    int v1 = map(constrain(co2History[i], 400, 2000), 400, 2000, 0, h - 4);
    int v2 = map(constrain(co2History[i+1], 400, 2000), 400, 2000, 0, h - 4);
    tft.drawLine(x + i*2, y + h - 2 - v1, x + (i+1)*2, y + h - 2 - v2, ST77XX_GREEN);
  }
}

/* ================= SETUP ================= */
void setup() {
  setCpuFrequencyMhz(160); 
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  spiTFT.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_GREENTAB); tft.setRotation(0); tft.fillScreen(ST77XX_BLACK);

  // --- SPLASH SCREEN  ---
  tft.drawRoundRect(5, 5, tft.width()-10, 45, 8, ST77XX_CYAN);
  tft.setTextSize(2); tft.setCursor(20, 15); tft.setTextColor(ST77XX_CYAN); tft.print("AIR SCAN");
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 70); tft.println("SYSTEM CHECK:");

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

  // Load MQTT
  preferences.begin("mqtt-conf", true);
  m_en = preferences.getBool("m_en", false);
  m_srv = preferences.getString("m_srv", "");
  m_port = preferences.getInt("m_port", 1883);
  m_user = preferences.getString("m_user", "");
  m_pass = preferences.getString("m_pass", "");
  preferences.end();

  tft.setCursor(10, tft.getCursorY()); tft.print(" > WiFi: ");
  if (connectToStoredWiFi()) {
    tft.setTextColor(ST77XX_GREEN); tft.println("\nCONNECTED");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if(m_en && m_srv != "") mqttClient.setServer(m_srv.c_str(), m_port);
  } else {
    tft.setTextColor(ST77XX_ORANGE); tft.println("\nAP MODE ACTIVE");
    WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID_DEF, AP_PASS_DEF);
  }

  server.on("/", [](){ server.send_P(200,"text/html",INDEX_HTML); });
  
  server.on("/clearwifi", [](){
    preferences.begin("wifi-list", false);
    preferences.clear();
    preferences.end();
    server.send(200, "text/html", "All WiFi networks erased. Restarting in AP Mode...");
    delay(2000);
    ESP.restart();
  });

  server.on("/status", [](){
    String mqSt = (!m_en) ? "OFF" : (mqttClient.connected() ? "OK" : "FAIL");
    String j="{";
    j += "\"co2\":"+String(co2)+",\"co2lvl\":\""+co2Level(co2)+"\",\"co2clr\":\""+levelColor(co2Level(co2))+"\",";
    j += "\"pm1\":"+String(pm1)+",\"pm1lvl\":\""+pmLevel(pm1)+"\",\"pm1clr\":\""+levelColor(pmLevel(pm1))+"\",";
    j += "\"pm25\":"+String(pm25)+",\"pm25lvl\":\""+pmLevel(pm25)+"\",\"pm25clr\":\""+levelColor(pmLevel(pm25))+"\",";
    j += "\"pm10\":"+String(pm10)+",\"pm10lvl\":\""+pmLevel(pm10)+"\",\"pm10clr\":\""+levelColor(pmLevel(pm10))+"\",";
    j += "\"temp\":"+(isnan(temperature)? "null" : String(temperature,1))+",\"hum\":"+(isnan(humidity)? "null" : String(humidity,0))+",";
    j += "\"mq\":\""+mqSt+"\",\"m_en\":"+String(m_en)+",";
    j += "\"ssid\":\""+(WiFi.status()==WL_CONNECTED?WiFi.SSID():"AP-Mode")+"\",\"ip\":\""+(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():WiFi.softAPIP().toString())+"\"";
    j += "}"; server.send(200,"application/json",j);
  });

  server.on("/scan", [](){
    int n = WiFi.scanNetworks(); String j = "[";
    for (int i=0; i<n; i++) { j += "{\"ssid\":\""+WiFi.SSID(i)+"\",\"rssi\":"+String(WiFi.RSSI(i))+"}"; if (i<n-1) j += ","; }
    j += "]"; server.send(200, "application/json", j);
  });

  server.on("/connect", HTTP_POST, [](){
    if (server.arg("ssid").length() > 0) saveWiFi(server.arg("ssid"), server.arg("pass"));
    
    preferences.begin("mqtt-conf", false);
    preferences.putBool("m_en", server.hasArg("m_en"));
    if (server.arg("m_srv").length() > 0) preferences.putString("m_srv", server.arg("m_srv"));
    if (server.arg("m_port").length() > 0) preferences.putInt("m_port", server.arg("m_port").toInt());
    if (server.arg("m_user").length() > 0) preferences.putString("m_user", server.arg("m_user"));
    if (server.arg("m_pass").length() > 0) preferences.putString("m_pass", server.arg("m_pass"));
    preferences.end();
    
    server.send(200, "text/html", "Restarting..."); delay(2000); ESP.restart();
  });
  
  server.begin();
  tft.setTextColor(ST77XX_CYAN); tft.setCursor(15, 145); tft.println("STARTING...");
  delay(1000); tft.fillScreen(ST77XX_BLACK);
  lastDisplayUpdate = millis(); 
}

void loop() {
  server.handleClient();
  if (scd30Detected && scd30.dataReady()) {
    scd30.read(); co2 = scd30.CO2; temperature = scd30.temperature; humidity = scd30.relative_humidity;
  }
  readPMS();

  if (WiFi.status() == WL_CONNECTED && m_en) { 
    reconnectMqtt(); mqttClient.loop(); 
    if (mqttClient.connected() && millis() - lastMqtt >= 10000) {
      lastMqtt = millis();
      String p = "{\"co2\":"+String(co2,0)+",\"pm1\":"+String(pm1)+",\"pm25\":"+String(pm25)+",\"pm10\":"+String(pm10)+",\"t\":"+String(temperature,2)+",\"h\":"+String(humidity,0)+"}";
      mqttClient.publish("air/status", p.c_str());
    }
  }

  if (millis() - lastGraphPoint >= graphInterval) {
    lastGraphPoint = millis();
    if (graphIdx < GRAPH_SAMPLES) co2History[graphIdx++] = (int)co2;
    else { for (int i=0; i<GRAPH_SAMPLES-1; i++) co2History[i] = co2History[i+1]; co2History[GRAPH_SAMPLES-1] = (int)co2; }
  }

  // --- CLOCK & UPTIME ---
  if (millis() - lastClockUpdate >= 1000) {
    lastClockUpdate = millis(); struct tm ti;
    tft.setCursor(10, 5);
    
    if(WiFi.status() == WL_CONNECTED && getLocalTime(&ti)) {
      // TIME
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
      char timeStr[15]; strftime(timeStr, sizeof(timeStr), "%I:%M %p", &ti);
      if (ti.tm_sec % 2 != 0) timeStr[2] = ' '; 
      tft.print(timeStr);
    } else { 
      // UPTIME
      tft.setTextSize(1);
      tft.setTextColor(0x7BEF, ST77XX_BLACK); // Серый цвет
      tft.print("UPTIME ");
      
      long s = millis() / 1000;
      int h = s / 3600;
      int m = (s % 3600) / 60;
      int sec = s % 60;
      
      tft.setTextSize(2);
      tft.setCursor(10, 15);
      tft.printf("%02d:%02d", h, m);
      
      if (sec % 2 == 0) tft.fillCircle(120, 10, 2, ST77XX_RED);
      else tft.fillCircle(120, 10, 2, ST77XX_BLACK);
    }
  }

  // --- MAIN UI REFRESH ---
  if (millis() - lastDisplayUpdate >= 5000) {
    lastDisplayUpdate = millis(); 
    int y = 32; tft.setTextSize(1);
    uint16_t cC = (co2<=800)?ST77XX_GREEN:(co2<=1200)?ST77XX_ORANGE:ST77XX_RED;
    tft.setTextColor(cC, ST77XX_BLACK); tft.setCursor(1, y);
    tft.printf("CO2: %.0f (%s)     ", co2, co2Level(co2).c_str());
    y += 12; drawCO2Graph(4, y, 120, 30); y += 35;
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); tft.setCursor(0, y);
    tft.printf("T:%.1fC H:%.0f%%  ", temperature, humidity); y += 15;
    auto dP = [&](const char* lbl, uint16_t v, int& yp) {
      String l = pmLevel(v); uint16_t c = (l=="Good")?ST77XX_GREEN:(l=="Fair")?ST77XX_ORANGE:ST77XX_RED;
      tft.setTextColor(c, ST77XX_BLACK); tft.setCursor(1, yp);
      tft.printf("%s:%u ", lbl, v);
      tft.setCursor(70, yp); tft.print(l + "    "); yp += 14;
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

  // PROGRESS BAR
  unsigned long elapsed = millis() - lastDisplayUpdate;
  if (elapsed <= 5000) {
    int bw = (elapsed * tft.width()) / 5000;
    tft.fillRect(0, tft.height() - 2, bw, 2, ST77XX_CYAN);
    tft.fillRect(bw, tft.height() - 2, tft.width() - bw, 2, ST77XX_BLACK);
  }
}
