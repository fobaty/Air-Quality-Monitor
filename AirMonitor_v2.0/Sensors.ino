//Sensor.ino
#include "Global.h"

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
      uint8_t buf[30];
      PMS.readBytes(buf, 30);
      pm1 = buf[8] << 8 | buf[9];
      pm25 = buf[10] << 8 | buf[11];
      pm10 = buf[12] << 8 | buf[13];
    }
  }
}