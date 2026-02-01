//Network.ino
#include "Global.h"

void saveWiFi(String ssid, String pass) {
  if (ssid == "") return;
  preferences.begin("wifi-list", false);
  if (preferences.getString("s0", "") == ssid) {
    preferences.putString("p0", pass);
    preferences.end();
    return;
  }
  for (int i = MAX_WIFI_NETWORKS - 1; i > 0; i--) {
    String s = preferences.getString(("s" + String(i - 1)).c_str(), "");
    String p = preferences.getString(("p" + String(i - 1)).c_str(), "");
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
    
    tft.printf("\nTrying [%d]: %s", i + 1, s.c_str());
    
    WiFi.disconnect(true); 
    WiFi.mode(WIFI_STA); 
    delay(100); 
    
    WiFi.begin(s.c_str(), p.c_str());
    unsigned long start = millis();
    
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
      delay(500); 
      tft.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      tft.println(" OK!");
      preferences.end(); 
      saveWiFi(s, p); 
      return true;
    }
  }
  
  WiFi.disconnect(true); 
  delay(100);
  
  preferences.end(); 
  return false;
}