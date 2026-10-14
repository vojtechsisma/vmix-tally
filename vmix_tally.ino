/*
  vMix Tally Light - ESP32 DevKit V1
  ----------------------------------
  - Pripojeni na vMix TCP API (port 8099), SUBSCRIBE TALLY
  - Webove rozhrani: stav, vyber inputu (s nazvy z vMixu), jas, IP vMixu,
    nazev zarizeni, identifikace (bliknuti LED), nastaveni WiFi
  - Bez WiFi / prvni spusteni: vlastni WiFi "Tally-XXXX" -> otevri 192.168.4.1
  - Po pripojeni: http://<nazev>.local  (napr. http://tally1.local) nebo IP ze Serial Monitoru

  Zapojeni: GPIO25 -> cervena LED (anoda), GPIO26 -> zelena LED (anoda),
            katody spolecne -> 100 R -> GND
            Napajeni: 1x Li-ion 18650 -> TP4056/DW01 modul -> SW1 -> LF33CV -> pin 3V3
            Baterie: BAT+ -> 10k -> GPIO34 -> 10k -> GND  (delic 1:2, mereni napeti)

  Arduino IDE, deska "ESP32 Dev Module", jadro esp32 3.x. Zadne externi knihovny.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <vector>
#include <esp_mac.h>

// ---------------- hardware ----------------
const int PIN_RED   = 25;
const int PIN_GREEN = 26;
const int PIN_BATT  = 34;      // ADC1 (funguje i se zapnutou WiFi), jen vstup
const int PIN_BOARD_LED = 2;   // modra LED na desce - blika pri slabe baterii

// ---------------- baterie ----------------
const float BATT_DIVIDER = 2.0;   // 10k / 10k
const float BATT_LOW     = 3.5;   // V - pod timto varovani
const float BATT_PRESENT = 2.5;   // V - pod timto = baterie nepripojena / SW1 vypnuty (napajeni z USB)
const int VMIX_PORT = 8099;

// Vysilaci vykon WiFi
// Moznosti: WIFI_POWER_19_5dBm (max), 15dBm, 11dBm, WIFI_POWER_8_5dBm, 7dBm, 5dBm
const wifi_power_t WIFI_TX_POWER = WIFI_POWER_19_5dBm;

// ---------------- stav ----------------
struct Config {
  String ssid, pass, vmixIp, name;
  int input  = 1;
  int bright = 255;
} cfg;

Preferences prefs;
WebServer   server(80);
DNSServer   dns;
WiFiClient  vmix;

enum TallyState { T_OFF = 0, T_PROGRAM = 1, T_PREVIEW = 2 };

bool     apMode       = false;
bool     vmixOk       = false;      // prijali jsme odpoved od vMixu
String   tallyStr;                  // posledni "0120..." retezec
String   rxLine;
long     xmlRemaining = -1;         // >=0 => prave cteme XML blok daneho poctu bajtu
String   xmlBuf;
std::vector<String> inputTitles;    // index 0 = input 1

unsigned long lastRx = 0, lastPing = 0, lastConnTry = 0, wifiLostAt = 0;
unsigned long identifyUntil = 0;

float         battV = 0;           // vyhlazene napeti baterie [V]
unsigned long lastBatt = 0, lastBattLog = 0;

// ---------------- LED ----------------
void ledsRaw(bool r, bool g) {
  ledcWrite(PIN_RED,   r ? cfg.bright : 0);
  ledcWrite(PIN_GREEN, g ? cfg.bright : 0);
}

TallyState currentTally() {
  if (!vmixOk || cfg.input < 1 || cfg.input > (int)tallyStr.length()) return T_OFF;
  char c = tallyStr[cfg.input - 1];
  if (c == '1') return T_PROGRAM;
  if (c == '2') return T_PREVIEW;
  return T_OFF;
}

// ---------------- baterie ----------------
bool battPresent() { return battV >= BATT_PRESENT; }
bool battLow()     { return battPresent() && battV < BATT_LOW; }

// odhad nabiti pro 1x Li-ion 18650 (Panasonic NCR18650GA, napeti pod malou zatezi)
// DW01 na TP4056 modulu odpoji clanek pri ~2,5 V, prakticky vybito uz od ~3,0 V
int battPercent(float v) {
  const float V[] = {3.00, 3.30, 3.50, 3.60, 3.70, 3.80, 3.90, 4.00, 4.10, 4.20};
  const int   P[] = {   0,    5,   15,   25,   45,   60,   75,   88,   95,  100};
  const int   N   = sizeof(V) / sizeof(V[0]);
  if (v <= V[0]) return 0;
  for (int i = 1; i < N; i++)
    if (v <= V[i]) return P[i-1] + (int)((v - V[i-1]) / (V[i] - V[i-1]) * (P[i] - P[i-1]));
  return 100;
}

void battLoop() {
  unsigned long now = millis();
  if (lastBatt && now - lastBatt < 500) return;
  lastBatt = now;
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(PIN_BATT);
  float raw = (sum / 16.0f) / 1000.0f * BATT_DIVIDER;
  battV = (battV == 0) ? raw : battV * 0.8f + raw * 0.2f;   // vyhlazeni (WiFi spicky)

  if (now - lastBattLog > 30000 || lastBattLog == 0) {
    lastBattLog = now;
    if (battPresent()) Serial.printf("Baterie: %.2f V (%d %%)%s\n", battV, battPercent(battV), battLow() ? " - SLABA!" : "");
    else               Serial.println("Baterie: nepripojena (USB napajeni)");
  }
}

void updateLeds() {
  // modra LED na desce: kratke bliknuti kazdou sekundu pri slabe baterii
  digitalWrite(PIN_BOARD_LED, battLow() && (millis() % 1000) < 80);

  unsigned long now = millis();

  if (now < identifyUntil) {                       // identifikace: rychle blikani obou
    bool on = (now / 150) % 2;
    ledsRaw(on, !on);
    return;
  }
  if (apMode && WiFi.status() != WL_CONNECTED) {   // ceka na nastaveni: stridani cervena/zelena
    bool on = (now / 700) % 2;
    ledsRaw(on, !on);
    return;
  }
  if (!vmixOk) {                                   // neni spojeni s vMixem: kratke zelene bliknuti kazde 2 s
    ledsRaw(false, (now % 2000) < 60);
    return;
  }
  TallyState t = currentTally();
  ledsRaw(t == T_PROGRAM, t == T_PREVIEW);
}

// ---------------- konfigurace ----------------
String macSuffix() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02x%02x", mac[4], mac[5]);
  return String(buf);
}

String defaultName() { return "tally-" + macSuffix(); }

void loadConfig() {
  prefs.begin("tally", true);
  cfg.ssid   = prefs.getString("ssid", "");
  cfg.pass   = prefs.getString("pass", "");
  cfg.vmixIp = prefs.getString("vmix", "");
  cfg.name   = prefs.getString("name", "");
  cfg.input  = prefs.getInt("input", 1);
  cfg.bright = prefs.getInt("bright", 255);
  prefs.end();
  if (cfg.name.isEmpty()) cfg.name = defaultName();
}

void saveConfig() {
  prefs.begin("tally", false);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putString("vmix", cfg.vmixIp);
  prefs.putString("name", cfg.name);
  prefs.putInt("input", cfg.input);
  prefs.putInt("bright", cfg.bright);
  prefs.end();
}

// hostname: jen a-z 0-9 a pomlcka
String sanitizeName(String s) {
  s.toLowerCase();
  String out;
  for (char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') out += c;
    else if (c == ' ' || c == '_') out += '-';
  }
  if (out.length() > 24) out = out.substring(0, 24);
  return out.isEmpty() ? defaultName() : out;
}

// ---------------- vMix ----------------
String xmlUnescape(String s) {
  s.replace("&lt;", "<");  s.replace("&gt;", ">");
  s.replace("&quot;", "\""); s.replace("&apos;", "'");
  s.replace("&amp;", "&");
  return s;
}

String attrOf(const String& tag, const char* name) {
  String key = String(" ") + name + "=\"";
  int p = tag.indexOf(key);
  if (p < 0) return "";
  p += key.length();
  int e = tag.indexOf('"', p);
  return e < 0 ? "" : tag.substring(p, e);
}

void parseXml() {
  int start = xmlBuf.indexOf("<inputs>");
  int end   = xmlBuf.indexOf("</inputs>");
  if (start < 0) return;
  if (end < 0) end = xmlBuf.length();

  std::vector<String> titles;
  int pos = start;
  while (true) {
    int t = xmlBuf.indexOf("<input ", pos);
    if (t < 0 || t >= end) break;
    int te = xmlBuf.indexOf('>', t);
    if (te < 0) break;
    String tag = xmlBuf.substring(t, te);
    int num = attrOf(tag, "number").toInt();
    String title = xmlUnescape(attrOf(tag, "title"));
    if (num >= 1 && num <= 200) {
      if ((int)titles.size() < num) titles.resize(num);
      titles[num - 1] = title;
    }
    pos = te;
  }
  inputTitles = titles;
  Serial.printf("vMix: nacteno %d inputu\n", (int)inputTitles.size());
}

void vmixSend(const char* cmd) {
  if (vmix.connected()) { vmix.print(cmd); vmix.print("\r\n"); }
}

void handleLine(String line) {
  line.trim();
  if (line.isEmpty()) return;

  if (line.startsWith("TALLY OK ")) {
    String s = line.substring(9);
    if (s != tallyStr) {
      tallyStr = s;
      Serial.printf("Tally: %s\n", tallyStr.c_str());
    }
    vmixOk = true;
  } else if (line.startsWith("XML ")) {
    xmlRemaining = line.substring(4).toInt();
    xmlBuf = "";
    xmlBuf.reserve(min(xmlRemaining, 65000L));
  } else if (line.startsWith("SUBSCRIBE OK")) {
    vmixOk = true;
  } else if (line.startsWith("VERSION OK")) {
    Serial.println("vMix: " + line);
  }
}

void vmixDisconnect() {
  vmix.stop();
  vmixOk = false;
  rxLine = "";
  xmlRemaining = -1;
  xmlBuf = "";
}

void vmixLoop() {
  if (WiFi.status() != WL_CONNECTED || cfg.vmixIp.isEmpty()) {
    if (vmix.connected() || vmixOk) vmixDisconnect();
    return;
  }

  unsigned long now = millis();

  if (!vmix.connected()) {
    if (vmixOk) { Serial.println("vMix: spojeni ztraceno"); vmixDisconnect(); }
    if (now - lastConnTry < 3000) return;
    lastConnTry = now;
    Serial.printf("vMix: pripojuji %s:%d ... ", cfg.vmixIp.c_str(), VMIX_PORT);
    if (vmix.connect(cfg.vmixIp.c_str(), VMIX_PORT, 1500)) {
      Serial.println("OK");
      vmix.setNoDelay(true);
      lastRx = lastPing = now;
      vmixSend("SUBSCRIBE TALLY");
      vmixSend("TALLY");     // okamzity aktualni stav
      vmixSend("XML");       // nazvy inputu
    } else {
      Serial.println("nepodarilo se");
    }
    return;
  }

  // cteni
  int budget = 4096;
  while (vmix.available() && budget-- > 0) {
    char c = vmix.read();
    lastRx = now;
    if (xmlRemaining >= 0) {
      if (xmlBuf.length() < 65000) xmlBuf += c;
      if (--xmlRemaining <= 0) { xmlRemaining = -1; parseXml(); xmlBuf = ""; }
      continue;
    }
    if (c == '\n') { handleLine(rxLine); rxLine = ""; }
    else if (rxLine.length() < 512) rxLine += c;
  }

  // keepalive: kazdych 5 s dotaz na tally, kdyz 15 s nic neprijde -> znovu pripojit
  if (now - lastPing > 5000) { lastPing = now; vmixSend("TALLY"); }
  if (now - lastRx > 15000) {
    Serial.println("vMix: bez odpovedi, pripojuji znovu");
    vmixDisconnect();
  }
}

// ---------------- WiFi ----------------
void startAP() {
  apMode = true;
  String apName = "Tally-" + macSuffix();
  WiFi.mode(cfg.ssid.isEmpty() ? WIFI_AP : WIFI_AP_STA);
  WiFi.softAP(apName.c_str());
  WiFi.setTxPower(WIFI_TX_POWER);
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("AP rezim: pripoj se na WiFi \"%s\" a otevri http://%s\n",
                apName.c_str(), WiFi.softAPIP().toString().c_str());
}

void stopAP() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_TX_POWER);
  apMode = false;
  Serial.println("AP vypnut, jedu jen na domaci WiFi");
}

void startWifi() {
  WiFi.setHostname(cfg.name.c_str());
  if (cfg.ssid.isEmpty()) { startAP(); return; }

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_TX_POWER);
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  Serial.printf("WiFi: pripojuji k \"%s\"", cfg.ssid.c_str());

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    bool on = (millis() / 250) % 2;
    ledsRaw(false, on);
    delay(50);
    if ((millis() - t0) % 1000 < 50) Serial.print(".");
  }
  ledsRaw(false, false);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi se nepodarilo pripojit -> zapinam vlastni AP (dal zkousim domaci WiFi)");
    startAP();
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  }
}

void wifiLoop() {
  // v AP rezimu: jakmile se chytne domaci WiFi a nikdo neni pripojen na AP, AP vypnout
  if (apMode && !cfg.ssid.isEmpty() && WiFi.status() == WL_CONNECTED && WiFi.softAPgetStationNum() == 0) {
    Serial.printf("WiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());
    stopAP();
  }
  // domaci WiFi dlouho pryc -> znovu zapnout AP, aby slo zarizeni nastavit
  if (!apMode && WiFi.status() != WL_CONNECTED) {
    if (wifiLostAt == 0) wifiLostAt = millis();
    if (millis() - wifiLostAt > 60000) { startAP(); WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str()); wifiLostAt = 0; }
  } else {
    wifiLostAt = 0;
  }
  if (apMode) dns.processNextRequest();
}

// ---------------- web ----------------
String jsonEscape(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

const char* stateName(TallyState t) {
  return t == T_PROGRAM ? "program" : t == T_PREVIEW ? "preview" : "off";
}

void handleStatus() {
  TallyState t = currentTally();
  String title = (cfg.input >= 1 && cfg.input <= (int)inputTitles.size()) ? inputTitles[cfg.input - 1] : "";
  String j = "{";
  j += "\"name\":\"" + jsonEscape(cfg.name) + "\",";
  j += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  j += "\"ssid\":\"" + jsonEscape(cfg.ssid) + "\",";
  j += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"ap\":" + String(apMode ? "true" : "false") + ",";
  j += "\"vmixIp\":\"" + jsonEscape(cfg.vmixIp) + "\",";
  j += "\"vmix\":" + String(vmixOk ? "true" : "false") + ",";
  j += "\"input\":" + String(cfg.input) + ",";
  j += "\"title\":\"" + jsonEscape(title) + "\",";
  j += "\"bright\":" + String(cfg.bright) + ",";
  j += "\"state\":\"" + String(stateName(t)) + "\",";
  j += "\"tally\":\"" + jsonEscape(tallyStr) + "\",";
  j += "\"batt\":" + String(battPresent() ? battV : 0, 2) + ",";
  j += "\"battPct\":" + String(battPresent() ? battPercent(battV) : -1) + ",";
  j += "\"battLow\":" + String(battLow() ? "true" : "false") + ",";
  j += "\"uptime\":" + String(millis() / 1000);
  j += "}";
  server.send(200, "application/json", j);
}

void handleInputs() {
  if (server.hasArg("refresh")) vmixSend("XML");
  String j = "[";
  for (size_t i = 0; i < inputTitles.size(); i++) {
    if (i) j += ",";
    j += "\"" + jsonEscape(inputTitles[i]) + "\"";
  }
  j += "]";
  server.send(200, "application/json", j);
}

void handleSet() {
  bool reconnect = false, renamed = false;
  if (server.hasArg("input"))  cfg.input  = constrain(server.arg("input").toInt(), 1, 999);
  if (server.hasArg("bright")) cfg.bright = constrain(server.arg("bright").toInt(), 5, 255);
  if (server.hasArg("vmix")) {
    String ip = server.arg("vmix"); ip.trim();
    if (ip != cfg.vmixIp) { cfg.vmixIp = ip; reconnect = true; }
  }
  if (server.hasArg("name")) {
    String n = sanitizeName(server.arg("name"));
    if (n != cfg.name) { cfg.name = n; renamed = true; }
  }
  saveConfig();
  if (reconnect) { vmixDisconnect(); lastConnTry = 0; }
  if (renamed) { MDNS.end(); MDNS.begin(cfg.name.c_str()); MDNS.addService("http", "tcp", 80); }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleWifi() {
  String newSsid = server.arg("ssid");
  newSsid.trim();
  String newPass = server.arg("pass");
  if (!(newPass.isEmpty() && newSsid == cfg.ssid)) cfg.pass = newPass;   // prazdne heslo u stejne site = ponechat stare
  cfg.ssid = newSsid;
  saveConfig();
  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  int count = 0;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.isEmpty()) continue;
    bool dup = false;
    for (int k = 0; k < i; k++) if (WiFi.SSID(k) == s) { dup = true; break; }
    if (dup) continue;
    if (count++) j += ",";
    j += "{\"ssid\":\"" + jsonEscape(s) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  j += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", j);
}

void handleIdentify() {
  identifyUntil = millis() + 3000;
  server.send(200, "application/json", "{\"ok\":true}");
}

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="cs"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>vMix Tally</title>
<style>
:root{--bg:#111418;--card:#1b2027;--line:#2a313b;--text:#e8ecf1;--mute:#8b96a5;--red:#e5322d;--green:#1fb04c;--acc:#3b82f6}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:520px;margin:0 auto;padding:16px}
h1{font-size:18px;margin:4px 0 2px}.sub{color:var(--mute);font-size:13px;margin-bottom:14px}
.tile{border-radius:16px;padding:28px 16px;text-align:center;background:#2a313b;transition:background .15s}
.tile.program{background:var(--red)}.tile.preview{background:var(--green)}
.tile .st{font-size:34px;font-weight:800;letter-spacing:2px}.tile .in{opacity:.9;margin-top:4px}
.row{display:flex;gap:8px;margin:12px 0}.pill{min-width:0;overflow-wrap:anywhere}.pill{flex:1;background:var(--card);border:1px solid var(--line);border-radius:10px;padding:8px 10px;font-size:13px}
.pill b{display:block;font-size:12px;color:var(--mute);font-weight:500}.ok{color:#4ade80}.bad{color:#f87171}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin:12px 0}
.card h2{font-size:14px;margin:0 0 10px;color:var(--mute);font-weight:600;text-transform:uppercase;letter-spacing:.5px}
label{display:block;font-size:13px;color:var(--mute);margin:10px 0 4px}
input,select,button{font:inherit;color:var(--text);background:#12161b;border:1px solid var(--line);border-radius:8px;padding:9px 10px;width:100%}
input[type=range]{padding:0}
.flex{display:flex;gap:8px}.flex>*{flex:1}
button{background:var(--acc);border:0;font-weight:600;cursor:pointer;margin-top:12px}
button.sec{background:#2a313b}
.msg{font-size:13px;color:var(--mute);min-height:18px;margin-top:6px}
</style></head><body><main>
<h1 id="name">vMix Tally</h1><div class="sub" id="sub">&nbsp;</div>

<div class="tile" id="tile"><div class="st" id="st">…</div><div class="in" id="in"></div></div>

<div class="row">
  <div class="pill"><b>WiFi</b><span id="wf">–</span></div>
  <div class="pill"><b>vMix</b><span id="vm">–</span></div>
  <div class="pill"><b>Baterie</b><span id="bt">–</span></div>
</div>

<div class="card"><h2>Tally</h2>
  <label>Input</label>
  <div class="flex"><select id="input"></select><button class="sec" style="margin:0;flex:0 0 auto;width:auto" onclick="loadInputs(1)">↻</button></div>
  <label>Jas LED: <span id="bv"></span></label>
  <input type="range" id="bright" min="5" max="255" oninput="bv.textContent=Math.round(this.value/2.55)+' %'" onchange="set({bright:this.value})">
  <button class="sec" onclick="identify()">Identifikovat</button>
</div>

<div class="card"><h2>Připojení</h2>
  <label>IP adresa vMix PC</label><input id="vmixIp" placeholder="192.168.1.50">
  <label>Název zařízení (adresa http://název.local)</label><input id="devname" placeholder="tally1">
  <button onclick="saveConn()">Uložit</button><div class="msg" id="m1"></div>
</div>

<div class="card"><h2>WiFi</h2>
  <label>Síť</label>
  <div class="flex"><select id="ssidSel" onchange="ssid.value=this.value"><option value="">— vyhledat —</option></select>
  <button class="sec" style="margin:0;flex:0 0 auto;width:auto" onclick="scan()">Hledat</button></div>
  <label>SSID</label><input id="ssid">
  <label>Heslo</label><input id="pass" type="password">
  <button onclick="saveWifi()">Uložit a restartovat</button><div class="msg" id="m2"></div>
</div>
</main>
<script>
const $=id=>document.getElementById(id);let first=true,titles=[];
async function j(u,o){const r=await fetch(u,o);return r.json()}
function form(d){return{method:'POST',body:new URLSearchParams(d)}}
async function set(d){await j('/api/set',form(d))}
async function identify(){await j('/api/identify')}
function fillInputs(sel){const s=$('input');const n=Math.max(titles.length,8,sel);s.innerHTML='';
  for(let i=1;i<=n;i++){const o=document.createElement('option');o.value=i;o.textContent=i+(titles[i-1]?' – '+titles[i-1]:'');s.appendChild(o)}
  s.value=sel}
async function loadInputs(refresh){if(refresh){await fetch('/api/inputs?refresh=1');await new Promise(r=>setTimeout(r,700))}
  titles=await j('/api/inputs');fillInputs(+$('input').value||1)}
$('input').onchange=e=>set({input:e.target.value});
async function poll(){try{const s=await j('/api/status');
  $('name').textContent=s.name;$('sub').textContent=(s.ap?'Režim nastavení · ':'')+(s.wifi?'http://'+s.name+'.local · '+s.ip:'');
  $('tile').className='tile '+s.state;
  $('st').textContent=!s.vmix?'NO vMix':s.state=='program'?'PROGRAM':s.state=='preview'?'PREVIEW':'OFF';
  $('in').textContent='Input '+s.input+(s.title?' – '+s.title:'');
  $('wf').innerHTML=s.wifi?'<span class=ok>●</span> '+s.ssid+' ('+s.rssi+' dBm)':'<span class=bad>●</span> nepřipojeno';
  $('bt').innerHTML=s.battPct<0?'USB':'<span class='+(s.battLow?'bad':'ok')+'>●</span> '+s.batt.toFixed(2)+' V · '+s.battPct+' %';
  $('vm').innerHTML=s.vmix?'<span class=ok>●</span> '+s.vmixIp:'<span class=bad>●</span> '+(s.vmixIp||'nenastaveno');
  if(first){first=false;$('vmixIp').value=s.vmixIp;$('devname').value=s.name;$('ssid').value=s.ssid;
    $('bright').value=s.bright;$('bv').textContent=Math.round(s.bright/2.55)+' %';fillInputs(s.input);loadInputs()}
  else if(document.activeElement!==$('input')&&+$('input').value!==s.input)fillInputs(s.input);
}catch(e){$('st').textContent='…'}}
async function saveConn(){await set({vmix:$('vmixIp').value.trim(),name:$('devname').value.trim()});$('m1').textContent='Uloženo';setTimeout(()=>$('m1').textContent='',2000);first=true}
async function scan(){$('m2').textContent='Hledám…';const l=await j('/api/scan');const s=$('ssidSel');s.innerHTML='<option value="">— vyber —</option>';
  l.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm)';s.appendChild(o)});$('m2').textContent=''}
async function saveWifi(){if(!$('ssid').value)return;$('m2').textContent='Ukládám, zařízení se restartuje…';
  try{await j('/api/wifi',form({ssid:$('ssid').value,pass:$('pass').value}))}catch(e){}}
poll();setInterval(poll,600);
</script></body></html>)HTML";

void setupWeb() {
  server.on("/",            HTTP_GET,  [] { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status",  HTTP_GET,  handleStatus);
  server.on("/api/inputs",  HTTP_GET,  handleInputs);
  server.on("/api/set",     HTTP_POST, handleSet);
  server.on("/api/wifi",    HTTP_POST, handleWifi);
  server.on("/api/scan",    HTTP_GET,  handleScan);
  server.on("/api/identify", HTTP_GET, handleIdentify);
  server.onNotFound([] {                     // captive portal v AP rezimu
    if (apMode) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });
  server.begin();
}

// ---------------- main ----------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== vMix Tally ===");

  ledcAttach(PIN_RED,   5000, 8);
  ledcAttach(PIN_GREEN, 5000, 8);
  pinMode(PIN_BOARD_LED, OUTPUT);
  analogSetPinAttenuation(PIN_BATT, ADC_11db);   // rozsah ~0-2,5 V linearne (4,2 V / 2 = 2,1 V)
  ledsRaw(false, false);

  loadConfig();
  Serial.printf("Nazev: %s, vMix: %s, input: %d\n", cfg.name.c_str(), cfg.vmixIp.c_str(), cfg.input);

  startWifi();
  if (MDNS.begin(cfg.name.c_str())) MDNS.addService("http", "tcp", 80);
  setupWeb();
  Serial.printf("Web: http://%s.local\n", cfg.name.c_str());
}

void loop() {
  wifiLoop();
  server.handleClient();
  vmixLoop();
  battLoop();
  updateLeds();
  delay(2);
}
