//Global.h
#ifndef GLOBAL_H
#define GLOBAL_H

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


/* === CONFIG === */
const char* const AP_SSID_DEF = "AIR-SCAN-CONFIG";
const char* const AP_PASS_DEF = "12345678";
const int WIFI_TIMEOUT = 10000;
const int MAX_WIFI_NETWORKS = 5;
const char* const ntpServer = "pool.ntp.org";

#define I2C_SDA 12
#define I2C_SCL 13
#define PMS_RX 11
#define PMS_TX 10
#define TFT_SCK 1
#define TFT_MOSI 2
#define TFT_RST 3
#define TFT_DC 4
#define TFT_CS 5
#define TFT_BL 6

/* === GLOBALS === */
extern WebServer server;
extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern Adafruit_SCD30 scd30;
extern HardwareSerial PMS;
extern Preferences preferences;
extern Adafruit_ST7735 tft;

extern String m_srv, m_user, m_pass;
extern int m_port;
extern bool m_en;
extern long gmtOffset_sec;
extern int daylightOffset_sec;

extern bool scd30Detected;
extern float co2, temperature, humidity;
extern uint16_t pm1, pm25, pm10;
extern unsigned long lastDisplayUpdate, lastMqtt, lastMqttRetry, lastGraphPoint, lastClockUpdate;
extern bool lastWasTime;
extern const char INDEX_HTML[] PROGMEM;

#define GRAPH_SAMPLES 60
extern int co2History[GRAPH_SAMPLES];
extern int graphIdx;
extern unsigned long graphInterval;

/* ===== FUNCTIONS===== */
void setupWebRoutes();
void saveWiFi(String ssid, String pass);
bool connectToStoredWiFi();
void drawCO2Graph(int x, int y, int w, int h);
void readPMS();
void reconnectMqtt();


/* === HELPERS === */
String pmLevel(uint16_t v);
String co2Level(float v);
String levelColor(String lvl);

#endif