//WebInterface.ino
#include "Global.h"

// HTML
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
input, select { width: 90%; padding: 12px; margin: 8px 0; border: 1px solid #ccc; border-radius: 6px; }
label { display: block; text-align: left; margin-top: 10px; font-weight: bold; color: #455a64; }
.btn-danger { background: #90a4ae; font-size: 11px; padding: 5px; margin-top: 5px; width: auto; display: inline-block; }
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
<label>GMT Offset (e.g. 2 or -8):</label><input type="number" name="gmt_h" id="gmt_h" step="1">
<label>Daylight Saving (DST):</label>
<select name="dst_en" id="dst_en"><option value="0">Disabled</option><option value="3600">Enabled (+1 Hour)</option></select>
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
if(!window.f){
document.getElementById("m_en").checked = d.m_en;
document.getElementById("gmt_h").value = d.gmt_h;
document.getElementById("dst_en").value = d.dst_s;
window.f=true;
}
});
}
function uB(id,v,l,c,n){let e=document.getElementById(id);e.style.background=c;e.innerText=n+": "+v+" ("+l+")";}
function scan(){let r=document.getElementById("scan-res");r.innerHTML="Scanning...";fetch("/scan").then(r=>r.json()).then(data=>{r.innerHTML="";data.forEach(n=>{let d=document.createElement("div");d.className="net-item";d.innerText=n.ssid+" ("+n.rssi+"dBm)";d.onclick=()=>{document.getElementById("ssid").value=n.ssid;};r.appendChild(d);});});}
setInterval(update, 5000); update();
</script></body></html>
)rawliteral";

// Function for initializing all server paths.
void setupWebRoutes() {
  server.on("/", []() { 
    server.send_P(200, "text/html", INDEX_HTML); 
  });

  // Web Server Endpoints
  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/clearwifi", []() {
    preferences.begin("wifi-list", false); preferences.clear(); preferences.end();
    server.send(200, "text/html", "Erased. Restarting..."); delay(2000); ESP.restart();
  });
  server.on("/status", []() {
    String mqSt = (!m_en) ? "OFF" : (mqttClient.connected() ? "OK" : "FAIL");
    String j = "{";
    j += "\"co2\":" + String(co2) + ",\"co2lvl\":\"" + co2Level(co2) + "\",\"co2clr\":\"" + levelColor(co2Level(co2)) + "\",";
    j += "\"pm1\":" + String(pm1) + ",\"pm1lvl\":\"" + pmLevel(pm1) + "\",\"pm1clr\":\"" + levelColor(pmLevel(pm1)) + "\",";
    j += "\"pm25\":" + String(pm25) + ",\"pm25lvl\":\"" + pmLevel(pm25) + "\",\"pm25clr\":\"" + levelColor(pmLevel(pm25)) + "\",";
    j += "\"pm10\":" + String(pm10) + ",\"pm10lvl\":\"" + pmLevel(pm10) + "\",\"pm10clr\":\"" + levelColor(pmLevel(pm10)) + "\",";
    j += "\"temp\":" + (isnan(temperature) ? "null" : String(temperature, 1)) + ",\"hum\":" + (isnan(humidity) ? "null" : String(humidity, 0)) + ",";
    j += "\"mq\":\"" + mqSt + "\",\"m_en\":" + String(m_en) + ",";
    j += "\"gmt_h\":" + String(gmtOffset_sec / 3600) + ",\"dst_s\":" + String(daylightOffset_sec) + ",";
    j += "\"ssid\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "AP-Mode") + "\",\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"";
    j += "}"; server.send(200, "application/json", j);
  });

  server.on("/scan", []() {
    // 1. Forcibly clear the old results.
    WiFi.scanDelete();
  
    // 2. Make sure that STA mode is ready
    WiFi.mode(WIFI_AP_STA);
    delay(50);

    // 3. Starting asynchronous scanning.
    int n = WiFi.scanNetworks(); 
  
    if (n == -1) {
      WiFi.disconnect();
      delay(100);
      n = WiFi.scanNetworks();
    }

    String j = "[";
    if (n > 0) {
      for (int i = 0; i < n; i++) {
        j += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        if (i < n - 1) j += ",";
      }
    }
    j += "]";
  
    WiFi.scanDelete(); 
    server.send(200, "application/json", j);
  });
  
  server.on("/connect", HTTP_POST, []() {
    if (server.arg("ssid").length() > 0) saveWiFi(server.arg("ssid"), server.arg("pass"));
    preferences.begin("mqtt-conf", false);
    preferences.putBool("m_en", server.hasArg("m_en"));
    if (server.arg("m_srv").length() > 0) preferences.putString("m_srv", server.arg("m_srv"));
    if (server.arg("m_port").length() > 0) preferences.putInt("m_port", server.arg("m_port").toInt());
    if (server.arg("m_user").length() > 0) preferences.putString("m_user", server.arg("m_user"));
    if (server.arg("m_pass").length() > 0) preferences.putString("m_pass", server.arg("m_pass"));
    if (server.hasArg("gmt_h")) preferences.putLong("gmt_off", server.arg("gmt_h").toInt() * 3600);
    if (server.hasArg("dst_en")) preferences.putInt("dst_off", server.arg("dst_en").toInt());
    preferences.end();
    server.send(200, "text/html", "Restarting..."); delay(2000); ESP.restart();
  });

  server.begin();
}