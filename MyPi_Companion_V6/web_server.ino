// ============================================================
//  web_server.ino — HTTP handlers, web server, AP setup/teardown, web OTA
//
//  startWebServer() registers all routes and starts the server.
//  It is called by _onConnected() (STA mode) AND by startAP().
//  The _webServerRunning flag prevents double-registration.
//
//  Route list:
//    GET  /                   — dashboard (status + 3×2 navigation grid)
//    GET  /networks           — Pi networks page (list + add new)
//    GET  /installonpi        — Install on Pi confirmation page
//    GET  /info               — System information page
//    GET  /connect            — connect Pi to named profile
//    GET  /status             — JSON status (Pi + ESP)
//    GET  /api/info           — JSON system diagnostics
//    POST /addwifi            — add Pi profile via USB serial
//    POST /api/installonpi    — set install target + pendingPiInstall flag
//    GET  /api/installcfg     — saved install mode / layout / user (no password)
//    GET  /profiles           — NVS profile manager page
//    GET  /api/nvs            — NVS slot JSON (shared pool)
//    GET  /api/piprofiles     — Pi-side profile list
//    POST /api/reqpiprofiles  — request fresh Pi profile list
//    POST /api/nvssave        — save NVS slot (ESP + Pi profile)
//    POST /api/nvsdel         — delete NVS slot
//    POST /api/nvsconnect     — tell Pi to connect NVS slot
//    POST /api/espconnect     — connect the ESP itself (slot, or ssid+pass) [deferred]
//    POST /api/espdisconnect  — drop the ESP STA link [deferred]
//    GET  /api/espstatus      — JSON: ESP STA state + last connect result
//    POST /api/espscan        — start async SSID scan on the ESP
//    GET  /api/espscan        — JSON: scan results (running flag + list)
//    GET  /terminal           — xterm.js terminal page
//    GET  /macros             — GUI macro & menu editor
//    GET  /update             — OTA firmware upload page
//    POST /update             — receive + flash firmware binary
//    GET  /api/files          — LittleFS file list (JSON)
//    POST /api/files/del      — delete a LittleFS file  (field: name)
//    POST /upload             — upload a file to LittleFS  (field: file)
//    GET  /files/<name>       — serve a file from LittleFS (via onNotFound)
//    GET  /storage            — Storage page (SD Share, RIGSETUP, file manager)
//    GET  /api/sd/status      — {sharing,onAtBoot,sdPresent}
//    POST /api/sd/share/start — expose SD card to the Pi over USB MSC
//    POST /api/sd/share/stop  — stop SD Share
//    POST /api/sd/boot        — enable SD Share on boot  (field: enable=0|1)
//    GET  /api/storage/ffat   — recursive FFat listing [{name,path,size,dir}]
//    GET  /api/storage/sd     — recursive SD listing   [{name,path,size,dir}]
//    POST /api/storage/delete — delete file/dir  (fields: path, fs=ffat|sd)
//    POST /api/storage/copy   — copy  (fields: srcPath, srcFs, dstPath, dstFs)
//    POST /api/storage/upload — multipart upload  (query: fs, dir; field: file)
//    GET  /api/storage/download — download a file (query: fs, path)
//    POST /api/rigsetup/start — present RIGSETUP disk (built from /rigsetup/)
//    POST /api/rigsetup/stop  — remove RIGSETUP disk
//    GET  /api/rigsetup/status — {active,files:[{name,size}]}
//    GET  /ap-profiles        — AP profile editor page
//    GET  /api/approfiles     — JSON: all 4 AP profiles + subnet labels (no passwords)
//    POST /api/approfile/save — save one AP profile (fields: slot, name, ssid, pass, subnet, nat)
//    POST /api/apstart        — start AP with given slot (field: slot) [sets pendingApStart]
//    GET  /api/apstatus       — JSON: AP/NAT/uplink diagnostics
//    GET  /logo.png           — MyPi Companion logo (PNG, PROGMEM)
// ============================================================

#include "page_html.h"   // PAGE_TMPL, NETWORKS_HTML, INSTALLPI_HTML, SYSINFO_HTML, OTA_HTML
#include "macros_html.h" // MACROS_HTML — GUI macro/menu editor at /macros
#include "storage_html.h" // STORAGE_HTML — storage page at /storage
#include "ap_profiles_html.h" // AP_PROFILES_HTML — AP profile editor at /ap-profiles
#include "logo_web.h"     // LOGO_WEB_PNG — MyPi Companion logo at /logo.png
#include <esp_wifi.h>     // channel / power-save / bandwidth diagnostics
#include <esp_wifi_ap_get_sta_list.h>   // esp_wifi_ap_get_sta_list_with_ip() for /api/apstatus

// Forward declaration (defined in display.ino)
const char* _ouiLookup(const uint8_t *mac);

// NAPT (internet sharing) state — see apNatService()
static bool      _natActive  = false;  // NAPT currently enabled on the AP netif
static bool      _natWanted  = false;  // active AP profile requests NAT
static char      _natStatus[24] = "off"; // human-readable status for display/web
static IPAddress _natDns;              // DNS server handed to AP clients

// Diagnostics defined in wifi_manager.ino / MyPi_Companion_V6.ino
// (wifi_manager.ino is concatenated after this file).
extern volatile uint32_t staDropCount;
extern volatile uint8_t  staLastReason;
extern volatile uint32_t staLastDropMs;
extern uint32_t loopMaxUs, loopMaxPrevUs;

// Install progress defined in hid.ino (concatenated before this file).
extern char     hidStatusMsg[2][24];
extern bool     hidStatusErr[2];
extern volatile bool hidInstallActive;


// ============================================================
//  BASIC HTTP HANDLERS
// ============================================================

// GET /logo.png — MyPi Companion logo served straight from PROGMEM
static void handleLogo() {
  server.sendHeader("Cache-Control", "max-age=3600");
  server.send_P(200, "image/png",
                (const char *)LOGO_WEB_PNG, LOGO_WEB_PNG_LEN);
}

void handleRoot() {
  server.send_P(200, "text/html", PAGE_TMPL);
}

void handleNetworks() {
  server.send_P(200, "text/html", NETWORKS_HTML);
}

void handleInstallPiPage() {
  server.send_P(200, "text/html", INSTALLPI_HTML);
}

void handleSysInfoPage() {
  server.send_P(200, "text/html", SYSINFO_HTML);
}

void handleConnect() {
  if (!server.hasArg("profile")) {
    server.send(400, "text/plain", "Missing profile parameter");
    return;
  }
  String profile = server.arg("profile");
  Serial.print("CMD:CONNECT:"); Serial.println(profile);
  server.send(200, "text/plain", String("Switching to: ") + profile);
}

void handleStatus() {
  String json = "{\"ssid\":\"";
  json += activeSSID;          // actual Wi-Fi SSID
  json += "\",\"conn\":\"";
  json += activeConn;          // NM connection name
  json += "\",\"ip\":\"";
  json += activeIP;
  json += "\",\"pi\":";
  json += piConnected ? "true" : "false";
  json += ",\"espssid\":\"";
  json += espSSID;
  json += "\",\"espip\":\"";
  json += espIP;
  json += "\",\"ap\":";
  json += apRunning ? "true" : "false";
  json += "}";
  server.send(200, "application/json", json);
}

// Append s to json with JSON string escaping (no surrounding quotes)
static void _jsonAppendEsc(String &json, const char *s) {
  for (; *s; s++) {
    char c = *s;
    if (c == '"' || c == '\\') { json += '\\'; json += c; }
    else if ((uint8_t)c < 0x20) json += ' ';
    else json += c;
  }
}

void handleApiInfo() {
  unsigned long ms = millis();
  String json = "{";
  json += "\"millis\":"; json += ms;
  json += ",\"heap\":";  json += ESP.getFreeHeap();
  json += ",\"sketch_size\":"; json += ESP.getSketchSize();
  json += ",\"free_sketch\":"; json += ESP.getFreeSketchSpace();
  json += ",\"pi_connected\":"; json += piConnected ? "true" : "false";
  json += ",\"pi_ssid\":\"";   json += activeSSID; json += "\"";
  json += ",\"pi_conn\":\"";  json += activeConn; json += "\"";
  json += ",\"pi_ip\":\"";     json += activeIP;   json += "\"";
  json += ",\"usb_port\":\"";  json += piPort;     json += "\"";
  json += ",\"esp_connected\":"; json += espConnected ? "true" : "false";
  json += ",\"esp_ssid\":\"";  json += espSSID;    json += "\"";
  json += ",\"esp_ip\":\"";    json += espIP;      json += "\"";
  // Firmware version + build date (__DATE__ "Oct 13 2026" -> "13 Oct 2026")
  char dateBuf[16];
  char mon_[4]; int day_; char year_[5];
  if (sscanf(__DATE__, "%3s %d %4s", mon_, &day_, year_) == 3) {
    snprintf(dateBuf, sizeof(dateBuf), "%d %s %s", day_, mon_, year_);
  } else {
    strlcpy(dateBuf, __DATE__, sizeof(dateBuf));
  }
  json += ",\"fw_version\":\"" FW_VERSION "\"";
  json += ",\"fw_date\":\""; json += dateBuf; json += "\"";
  // Pi system info (STATUS:SYS) — every key the daemon sent, in its order
  json += ",\"pi_sys_age\":";
  json += piSysMs ? (long)((millis() - piSysMs) / 1000) : -1L;
  json += ",\"pi_sys\":[";
  for (int i = 0; i < piSysCount; i++) {
    if (i) json += ',';
    json += "[\""; _jsonAppendEsc(json, piSys[i].key);
    json += "\",\""; _jsonAppendEsc(json, piSys[i].val);
    json += "\"]";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleAddWifi() {
  if (!server.hasArg("ssid") || server.arg("ssid").isEmpty()) {
    server.send(400, "text/plain", "ERROR: SSID missing");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();
  Serial.print("CMD:ADDWIFI:");
  Serial.print(ssid);
  Serial.print(":");
  Serial.println(pass);
  server.send(200, "text/plain", String("Sending \"") + ssid + "\" to Pi...");
}

// GET /api/installcfg — saved install target (never the password)
void handleApiInstallCfg() {
  String user = piUser;
  user.replace("\\", "\\\\");
  user.replace("\"", "\\\"");
  String json = String("{\"mode\":") + piInstallMode +
                ",\"layout\":" + piLayoutForce +
                ",\"user\":\"" + user + "\"}";
  server.send(200, "application/json", json);
}

static bool _isPrintableAscii(const String &s) {
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

// POST /api/installonpi — triggers HID installer via loop()
//   mode=desktop|cli, user=<name>, pass=<password>, layout=-1..4
//   Without parameters the saved mode is used (old v5 behaviour).
void handleApiInstallOnPi() {
  if (pendingPiInstall) {
    server.send(409, "text/plain", "An installation is already pending.");
    return;
  }
  if (server.hasArg("mode")) {
    uint8_t mode   = server.arg("mode") == "cli" ? 1 : 0;
    int     layout = server.hasArg("layout") ? server.arg("layout").toInt() : -1;
    if (layout < -1 || layout > 4) layout = -1;
    String  user   = server.arg("user");
    String  pass   = server.arg("pass");

    if (mode == 1) {
      if (user.isEmpty() || pass.isEmpty()) {
        server.send(400, "text/plain", "CLI mode needs both username and password.");
        return;
      }
      if (user.length() >= sizeof(piUser) || pass.length() >= sizeof(piPass) ||
          !_isPrintableAscii(user) || !_isPrintableAscii(pass)) {
        server.send(400, "text/plain",
          "Username/password must be printable ASCII (max 32/64 chars) - the HID keyboard cannot type other characters.");
        return;
      }
      strlcpy(piUser, user.c_str(), sizeof(piUser));
      strlcpy(piPass, pass.c_str(), sizeof(piPass));   // RAM only
    }
    piInstallMode = mode;
    piLayoutForce = (int8_t)layout;

    Preferences p; p.begin("rigcfg", false);
    p.putUChar("instMode", piInstallMode);
    p.putChar("instLayout", piLayoutForce);
    if (mode == 1) p.putString("instUser", piUser);
    p.end();
  }
  pendingPiInstall = true;
  server.send(200, "text/plain",
    piInstallMode == 1
      ? "CLI installation started via USB HID - watch the ESP32 display (takes 1-2 min)."
      : "Installation started via USB HID - watch the Pi display.");
}


// GET /api/installstatus — live install progress for the web page
void handleApiInstallStatus() {
  char line0[28], line1[28];
  snprintf(line0, sizeof(line0), "%s", hidStatusMsg[0]);
  snprintf(line1, sizeof(line1), "%s", hidStatusMsg[1]);
  String json = String("{\"running\":") + (hidInstallActive ? "true" : "false") +
                ",\"err\":"  + ((hidStatusErr[0] || hidStatusErr[1]) ? "true" : "false") +
                ",\"line0\":\"" + line0 + "\"" +
                ",\"line1\":\"" + line1 + "\"}";
  server.send(200, "application/json", json);
}

// ============================================================
//  PROFILE MANAGER HANDLERS  (NVS slots — shared pool)
// ============================================================
void handleProfiles() {
  server.send_P(200, "text/html", PROFILES_HTML);
}

void handleApiNvs() {
  String json = "[";
  for (int i = 0; i < MAX_NVS_SLOTS; i++) {
    if (i > 0) json += ",";
    NvsProfile &p = nvsProfiles[i];
    json += "{\"slot\":"; json += i;
    json += ",\"valid\":"; json += p.valid ? "true" : "false";
    if (p.valid) {
      json += ",\"ssid\":\""; json += p.ssid; json += "\"";
      json += ",\"pass\":\""; json += p.pass; json += "\"";
      json += ",\"dhcp\":";   json += p.dhcp ? "true" : "false";
      json += ",\"ip\":\"";   json += p.ip;   json += "\"";
      json += ",\"sn\":\"";   json += p.sn;   json += "\"";
      json += ",\"gw\":\"";   json += p.gw;   json += "\"";
      json += ",\"dns\":\"";  json += p.dns;  json += "\"";
    }
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleApiPiProfiles() {
  String json = "[";
  for (int i = 0; i < numPiProfiles; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\""; json += piProfiles[i].ssid;
    json += "\",\"pass\":\""; json += piProfiles[i].pass; json += "\"}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// GET /api/piconns — NetworkManager connection names (from PROFILES:, sent by
// the daemon after HELLO and after ADDWIFI). These are what CMD:CONNECT needs
// and what STATUS:CONN reports; on newer Pi OS they look like
// "netplan-wlan0-<SSID>", which is not the SSID from /api/piprofiles.
void handleApiPiConns() {
  // Empty list while the Pi is connected = we missed PROFILES:. Re-send HELLO
  // (daemon answers with ACK + PROFILES + STATUS), at most every 10 s.
  static unsigned long lastAsk = 0;
  if (numDynProfiles == 0 && piConnected && millis() - lastAsk > 10000UL) {
    lastAsk = millis();
    Serial.println("HELLO");
  }
  String json = "[";
  for (int i = 0; i < numDynProfiles; i++) {
    if (i > 0) json += ",";
    json += "\"";
    for (const char *c = dynProfiles[i]; *c; c++) {
      if (*c == '"' || *c == '\\') json += '\\';
      json += *c;
    }
    json += "\"";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleApiReqPiProfiles() {
  piProfilesFresh = false;
  Serial.println("CMD:GET_PROFILES_FULL");
  server.send(200, "text/plain", "OK");
}

void handleApiNvsSave() {
  if (!server.hasArg("slot") || !server.hasArg("ssid")) {
    server.send(400, "text/plain", "ERR: missing params"); return;
  }
  int slot = server.arg("slot").toInt();
  if (slot < 0 || slot >= MAX_NVS_SLOTS) {
    server.send(400, "text/plain", "ERR: invalid slot"); return;
  }
  String ssid = server.arg("ssid"); ssid.trim();
  if (ssid.isEmpty()) { server.send(400, "text/plain", "ERR: SSID empty"); return; }
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  bool   dhcp = !server.hasArg("dhcp") || server.arg("dhcp") == "1";
  String ip   = server.hasArg("ip")  ? server.arg("ip")  : "";
  String sn   = server.hasArg("sn")  ? server.arg("sn")  : "255.255.255.0";
  String gw   = server.hasArg("gw")  ? server.arg("gw")  : "";
  String dns  = server.hasArg("dns") ? server.arg("dns") : "";
  if (sn.isEmpty()) sn = "255.255.255.0";
  saveNvsSlot(slot, ssid.c_str(), pass.c_str(), dhcp,
              ip.c_str(), sn.c_str(), gw.c_str(), dns.c_str());
  server.send(200, "text/plain", String("Saved to slot ") + (slot + 1));
}

void handleApiNvsDel() {
  if (!server.hasArg("slot")) {
    server.send(400, "text/plain", "ERR: missing slot"); return;
  }
  int slot = server.arg("slot").toInt();
  if (slot < 0 || slot >= MAX_NVS_SLOTS) {
    server.send(400, "text/plain", "ERR: invalid slot"); return;
  }
  deleteNvsSlot(slot);
  server.send(200, "text/plain", String("Deleted slot ") + (slot + 1));
}

void handleApiNvsConnect() {
  if (!server.hasArg("slot")) {
    server.send(400, "text/plain", "ERR: missing slot"); return;
  }
  int slot = server.arg("slot").toInt();
  if (slot < 0 || slot >= MAX_NVS_SLOTS || !nvsProfiles[slot].valid) {
    server.send(400, "text/plain", "ERR: invalid or empty slot"); return;
  }
  NvsProfile &p = nvsProfiles[slot];
  Serial.print("CMD:ADDWIFI_FULL:");
  Serial.print(p.ssid); Serial.print("|");
  Serial.print(p.pass); Serial.print("|");
  Serial.print(p.dhcp ? "1" : "0"); Serial.print("|");
  Serial.print(p.ip);   Serial.print("|");
  Serial.print(p.sn);   Serial.print("|");
  Serial.print(p.gw);   Serial.print("|");
  Serial.println(p.dns);
  server.send(200, "text/plain",
              String("Connecting: ") + p.ssid + " (slot " + (slot + 1) + ")");
}


// ============================================================
//  ESP STA CONNECT (web)  — connects the ESP itself, unlike
//  /api/nvsconnect which asks the Pi to join a network.
// ============================================================

// POST /api/espconnect  slot=N            → connect saved NVS slot N (0-based)
//                       ssid=..&pass=..   → save to NVS (same SSID slot,
//                                           else first free, else last) then connect
void handleApiEspConnect() {
  if (espConnecting || pendingEspConnect >= 0) {
    server.send(409, "text/plain", "ERR: a connect is already in progress"); return;
  }
  int slot = -1;
  if (server.hasArg("ssid") && server.arg("ssid").length() > 0) {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() > 32) { server.send(400, "text/plain", "ERR: SSID too long"); return; }
    if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) {
      server.send(400, "text/plain", "ERR: password must be 8-63 chars (or empty for open)"); return;
    }
    for (int i = 0; i < MAX_NVS_SLOTS && slot < 0; i++)
      if (nvsProfiles[i].valid && ssid == nvsProfiles[i].ssid) slot = i;
    for (int i = 0; i < MAX_NVS_SLOTS && slot < 0; i++)
      if (!nvsProfiles[i].valid) slot = i;
    if (slot < 0) slot = MAX_NVS_SLOTS - 1;
    saveNvsSlot(slot, ssid.c_str(), pass.c_str(),
                true /*dhcp*/, "", "255.255.255.0", "", "");
    loadNvsProfiles();
  } else {
    slot = server.arg("slot").toInt();
    if (!server.hasArg("slot") || slot < 0 || slot >= MAX_NVS_SLOTS || !nvsProfiles[slot].valid) {
      server.send(400, "text/plain", "ERR: invalid or empty slot"); return;
    }
  }
  pendingEspConnect = slot;   // processed in loop() after this reply is sent
  String j = "{\"ok\":true,\"slot\":"; j += slot;
  j += ",\"ssid\":\""; _jsonAppendEsc(j, nvsProfiles[slot].ssid); j += "\"}";
  server.send(200, "application/json", j);
}

// POST /api/espdisconnect
void handleApiEspDisconnect() {
  if (!apRunning) {
    // Without the AP the STA link is the only way to reach this page.
    server.send(409, "text/plain", "ERR: AP not running - disconnecting would cut this page off");
    return;
  }
  pendingEspDisconnect = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

// GET /api/espstatus
void handleApiEspStatus() {
  String j = "{";
  j += "\"connected\":";  j += espConnected ? "true" : "false";
  j += ",\"connecting\":"; j += (espConnecting || pendingEspConnect >= 0) ? "true" : "false";
  j += ",\"ssid\":\"";    _jsonAppendEsc(j, espConnected ? espSSID : ""); j += "\"";
  j += ",\"ip\":\"";      j += espConnected ? espIP : ""; j += "\"";
  j += ",\"slot\":";      j += espActiveSlot;
  j += ",\"rssi\":";      j += espConnected ? (int)WiFi.RSSI() : 0;
  j += ",\"channel\":";   j += (int)WiFi.channel();
  j += ",\"target\":\"";  _jsonAppendEsc(j, espConnTarget); j += "\"";
  j += ",\"result\":";    j += espConnResult;   // 0 none, 1 ok, 2 failed
  j += ",\"ap\":";        j += apRunning ? "true" : "false";
  j += ",\"via_ap\":";    // is this request coming in over the AP?
  j += (apRunning && server.client().localIP() == WiFi.softAPIP()) ? "true" : "false";
  j += "}";
  server.send(200, "application/json", j);
}

// POST /api/espscan — start; GET /api/espscan — results
void handleApiEspScanStart() {
  if (espConnecting) { server.send(409, "text/plain", "ERR: busy connecting"); return; }
  if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    WiFi.scanDelete();
    wifiScanStart();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleApiEspScanGet() {
  int n = WiFi.scanComplete();
  String j = "{\"running\":";
  j += (n == WIFI_SCAN_RUNNING) ? "true" : "false";
  j += ",\"list\":[";
  bool first = true;
  for (int i = 0; i < n && i < 30; i++) {
    String ss = WiFi.SSID(i);
    if (ss.length() == 0) continue;   // hidden networks: type the SSID manually
    if (!first) j += ",";
    first = false;
    j += "{\"ssid\":\""; _jsonAppendEsc(j, ss.c_str());
    j += "\",\"rssi\":"; j += WiFi.RSSI(i);
    j += ",\"ch\":";     j += WiFi.channel(i);
    j += ",\"open\":";   j += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "true" : "false";
    j += "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}


// ============================================================
//  OTA HANDLERS  (web-based, replaces ArduinoOTA)
// ============================================================
static size_t _otaTotal   = 0;
static size_t _otaWritten = 0;
// -1=idle  0-95=flashing (5% steps)  100=done  -2=error
static int    _otaPct     = -1;
static char   _otaErr[48] = "";

void handleOtaPage() {
  server.send_P(200, "text/html", OTA_HTML);
}

void handleOtaUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    _otaTotal   = 0;
    _otaWritten = 0;
    _otaPct     = 0;
    _otaErr[0]  = '\0';
    drawOtaScreen(-1);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      _otaPct = -2;
      strlcpy(_otaErr, Update.errorString(), sizeof(_otaErr));
      drawOtaError("Begin failed");
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    size_t written = Update.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      drawOtaError("Write error");
    }
    _otaWritten += written;
    if (_otaTotal == 0 && server.hasHeader("Content-Length")) {
      _otaTotal = (size_t)server.header("Content-Length").toInt();
    }
    if (_otaTotal > 0) {
      int pct = constrain((int)((uint64_t)_otaWritten * 100 / _otaTotal), 0, 99);
      int step = pct / 5;                 // 0..19
      if (step > _otaPct / 5) {           // new 5 % threshold crossed
        _otaPct = step * 5;
        drawOtaScreen(_otaPct);           // only redraw at 0/5/10/.../95 %
      }
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      _otaPct = 100;
      drawOtaScreen(101);
    } else {
      _otaPct = -2;
      strlcpy(_otaErr, Update.errorString(), sizeof(_otaErr));
      char errBuf[40];
      snprintf(errBuf, sizeof(errBuf), "End: %s", Update.errorString());
      drawOtaError(errBuf);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    _otaPct = -2;
    strlcpy(_otaErr, "Aborted", sizeof(_otaErr));
    Update.abort();
    drawOtaError("Aborted");
  }
}

// GET /api/otaprogress — browser polls this every 500 ms to track flash progress.
// Returns {"pct":N,"busy":true/false,"err":"..."} where pct is -1 (idle),
// 0/5/10/.../95 (flashing, 5 % steps), or 100 (done).
void handleOtaProgress() {
  char pct_s[8];
  int  pct  = (_otaPct < 0) ? -1 : _otaPct;
  bool busy = (_otaPct >= 0 && _otaPct < 100);
  // escape single quotes in error string for JSON safety
  char err[48]; strlcpy(err, _otaErr, sizeof(err));
  char buf[100];
  snprintf(buf, sizeof(buf), "{\"pct\":%d,\"busy\":%s,\"err\":\"%s\"}",
           pct, busy ? "true" : "false", err);
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", buf);
}

// Runs after the whole request has been received — always sends exactly
// one response, so the browser never hangs (e.g. when Update.begin failed).
void handleOtaDone() {
  if (Update.isFinished() && !Update.hasError()) {
    server.send(200, "text/plain", "OK");
    delay(1200);
    ESP.restart();
  } else {
    String err = Update.hasError() ? String(Update.errorString())
                                   : String("No firmware received");
    server.send(500, "text/plain", err);
  }
}


// ============================================================
//  QUICK BUTTON  API  (/api/quickbtns, /api/quickbtns/save)
//
//  GET  /api/quickbtns        — return all 25 slot→macroId pairs as JSON
//  POST /api/quickbtns/save   — persist one slot (params: slot, macroid)
// ============================================================

static void handleGetQuickBtns() {
  String json = "[";
  for (int i = 0; i < 25; i++) {
    if (i > 0) json += ",";
    json += "{\"slot\":" + String(i) +
            ",\"macroid\":" + String(quickBtnIds[i]) + "}";
  }
  json += "]";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

static void handleSaveQuickBtn() {
  if (!server.hasArg("slot") || !server.hasArg("macroid")) {
    server.send(400, "text/plain", "ERR: missing params");
    return;
  }
  int slot    = server.arg("slot").toInt();
  int macroid = server.arg("macroid").toInt();
  if (slot < 0 || slot >= 25) {
    server.send(400, "text/plain", "ERR: invalid slot");
    return;
  }
  saveQuickBtnSlot(slot, (int32_t)macroid);
  server.send(200, "text/plain", "OK");
}


// ============================================================
//  STORAGE / SD SHARE / RIGSETUP  API
//
//  Filesystem selector "fs": "ffat" (internal) or "sd" (SD card).
//  SD file operations are refused while SD Share is active,
//  because the Pi owns the card's filesystem at that time.
//  POST parameters are form fields (URLSearchParams), like the
//  rest of this server.
// ============================================================

// Resolve "ffat" / "sd" to a mounted filesystem, or send an error
// response and return nullptr.
static fs::FS *_stFs(const String &name) {
  if (name == "ffat") {
    if (!_fatOk) { server.send(503, "text/plain", "ERR: FFat not mounted"); return nullptr; }
    return &FFat;
  }
  if (name == "sd") {
    if (sdIsShareActive()) { server.send(409, "text/plain", "ERR: SD Share active — stop sharing first"); return nullptr; }
    if (!sdMount()) { server.send(503, "text/plain", "ERR: no SD card"); return nullptr; }
    return &SD_MMC;
  }
  server.send(400, "text/plain", "ERR: fs must be ffat or sd");
  return nullptr;
}

// Absolute path without ".." components.
static bool _stPathOk(const String &p) {
  return p.length() > 1 && p[0] == '/' && p.indexOf("..") < 0;
}

static String _stBaseName(const String &p) {
  int s = p.lastIndexOf('/');
  return s >= 0 ? p.substring(s + 1) : p;
}

// Create all parent directories of path.
static void _stMkParents(fs::FS &fs, const String &path) {
  int pos = 1;
  while ((pos = path.indexOf('/', pos)) > 0) {
    String d = path.substring(0, pos);
    if (!fs.exists(d)) fs.mkdir(d);
    pos++;
  }
}

static bool _stCopyFile(fs::FS &sfs, const String &src, fs::FS &dfs, const String &dst) {
  File in = sfs.open(src, FILE_READ);
  if (!in) return false;
  _stMkParents(dfs, dst);
  File out = dfs.open(dst, FILE_WRITE);
  if (!out) { in.close(); return false; }
  static uint8_t buf[4096];
  bool ok = true;
  while (in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (n == 0) break;
    if (out.write(buf, n) != n) { ok = false; break; }
    yield();
  }
  in.close(); out.close();
  if (!ok) dfs.remove(dst);
  return ok;
}

static bool _stCopyRec(fs::FS &sfs, const String &src, fs::FS &dfs,
                       const String &dst, int depth) {
  File f = sfs.open(src);
  if (!f) return false;
  if (!f.isDirectory()) { f.close(); return _stCopyFile(sfs, src, dfs, dst); }
  if (depth > 6) { f.close(); return false; }
  _stMkParents(dfs, dst + "/");
  bool ok = true;
  for (File c = f.openNextFile(); c; c = f.openNextFile()) {
    String name = _stBaseName(String(c.name()));
    c.close();
    if (!_stCopyRec(sfs, src + "/" + name, dfs, dst + "/" + name, depth + 1)) ok = false;
  }
  f.close();
  return ok;
}

// ── SD Share ──────────────────────────────────────────────────
static void handleSdStatus() {
  String json = "{\"sharing\":";
  json += sdIsShareActive() ? "true" : "false";
  json += ",\"onAtBoot\":";  json += sdIsOnBoot()  ? "true" : "false";
  json += ",\"sdPresent\":"; json += sdIsMounted() ? "true" : "false";
  json += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

static void handleSdShareStart() {
  if (sdShareStart()) server.send(200, "text/plain", "OK");
  else                server.send(503, "text/plain", "ERR: no SD card");
}

static void handleSdShareStop() {
  sdShareStop();
  server.send(200, "text/plain", "OK");
}

static void handleSdBoot() {
  if (!server.hasArg("enable")) { server.send(400, "text/plain", "ERR: missing enable"); return; }
  String v = server.arg("enable");
  sdSetOnBoot(v == "1" || v == "true");
  server.send(200, "text/plain", "OK");
}

// ── File manager ──────────────────────────────────────────────
static void _stSendList(const char *fsName) {
  fs::FS *fs = _stFs(fsName);
  if (!fs) return;
  if (fs == &FFat && !FFat.exists("/rigsetup")) FFat.mkdir("/rigsetup");
  String json = "[";
  bool first = true;
  sdListDir(*fs, "/", json, first);
  json += "]";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

static void handleStorageListFfat() { _stSendList("ffat"); }
static void handleStorageListSd()   { _stSendList("sd"); }

static void handleStorageDelete() {
  String path = server.arg("path");
  if (!_stPathOk(path)) { server.send(400, "text/plain", "ERR: bad path"); return; }
  fs::FS *fs = _stFs(server.arg("fs"));
  if (!fs) return;
  bool ok = (fs == &SD_MMC) ? sdDeleteFile(path.c_str())
                            : _fsRemoveRec(*fs, path, 0);
  if (ok) server.send(200, "text/plain", "Deleted");
  else    server.send(500, "text/plain", "ERR: delete failed");
}

static void handleStorageCopy() {
  String src = server.arg("srcPath"), dst = server.arg("dstPath");
  if (!_stPathOk(src) || !_stPathOk(dst)) { server.send(400, "text/plain", "ERR: bad path"); return; }
  fs::FS *sfs = _stFs(server.arg("srcFs"));
  if (!sfs) return;
  fs::FS *dfs = _stFs(server.arg("dstFs"));
  if (!dfs) return;
  if (sfs == dfs && (dst == src || dst.startsWith(src + "/"))) {
    server.send(400, "text/plain", "ERR: destination inside source"); return;
  }
  if (!sfs->exists(src)) { server.send(404, "text/plain", "ERR: source not found"); return; }
  if (_stCopyRec(*sfs, src, *dfs, dst, 0)) server.send(200, "text/plain", "Copied");
  else                                     server.send(500, "text/plain", "ERR: copy failed");
}

static File   _stUpFile;
static fs::FS *_stUpFs = nullptr;
static String _stUpPath;
static bool   _stUpOk = false;

static void handleStorageUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    _stUpOk = false;
    _stUpFs = nullptr;
    String fsName = server.arg("fs");
    String dir    = server.hasArg("dir") ? server.arg("dir") : String("/");
    if (!dir.startsWith("/") || dir.indexOf("..") >= 0) return;
    if (!dir.endsWith("/")) dir += "/";
    String name = _stBaseName(up.filename);
    if (name.isEmpty()) return;
    if (fsName == "ffat" && _fatOk) _stUpFs = &FFat;
    else if (fsName == "sd" && !sdIsShareActive() && sdMount()) _stUpFs = &SD_MMC;
    if (!_stUpFs) return;
    _stUpPath = dir + name;
    _stMkParents(*_stUpFs, _stUpPath);
    _stUpFile = _stUpFs->open(_stUpPath, FILE_WRITE);
    _stUpOk = (bool)_stUpFile;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (_stUpOk && _stUpFile.write(up.buf, up.currentSize) != up.currentSize) _stUpOk = false;
  } else if (up.status == UPLOAD_FILE_END) {
    if (_stUpFile) _stUpFile.close();
    if (!_stUpOk && _stUpFs) _stUpFs->remove(_stUpPath);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (_stUpFile) _stUpFile.close();
    if (_stUpFs) _stUpFs->remove(_stUpPath);
    _stUpOk = false;
  }
}

static void handleStorageUploadDone() {
  if (_stUpOk) server.send(200, "text/plain", String("Uploaded ") + _stUpPath);
  else         server.send(500, "text/plain", "ERR: upload failed (bad fs/dir, no card, or SD Share active)");
}

static void handleStorageDownload() {
  String path = server.arg("path");
  if (!_stPathOk(path)) { server.send(400, "text/plain", "ERR: bad path"); return; }
  fs::FS *fs = _stFs(server.arg("fs"));
  if (!fs) return;
  File f = fs->open(path, FILE_READ);
  if (!f || f.isDirectory()) { server.send(404, "text/plain", "ERR: not found"); return; }
  server.sendHeader("Content-Disposition",
                    String("attachment; filename=\"") + _stBaseName(path) + "\"");
  server.streamFile(f, "application/octet-stream");
  f.close();
}

// ── RIGSETUP ──────────────────────────────────────────────────
static void handleRigsetupForceCopy() {
  if (mscBackendIsRigsetup()) {
    // Can't overwrite files while they're mounted as USB disk.
    server.send(409, "text/plain", "ERR: stop RIGSETUP disk first");
    return;
  }
  int n = forcePopulateRigsetupFiles();
  if (n == (int)RIGSETUP_FILE_COUNT) {
    server.send(200, "text/plain",
                String("OK: ") + n + " files restored from firmware");
  } else {
    server.send(500, "text/plain",
                String("ERR: only ") + n + "/" + RIGSETUP_FILE_COUNT + " files written");
  }
}

static void handleRigsetupStart() {
  if (sdIsShareActive()) sdShareStop();
  if (mscSetBackendRigsetup()) server.send(200, "text/plain", "OK");
  else                         server.send(500, "text/plain", "ERR: RIGSETUP build failed");
}

static void handleRigsetupStop() {
  if (mscBackendIsRigsetup()) mscSetBackendNone();
  server.send(200, "text/plain", "OK");
}

static void handleRigsetupStatus() {
  String json = "{\"active\":";
  json += mscBackendIsRigsetup() ? "true" : "false";
  json += ",\"files\":[";
  bool first = true;
  File dir = FFat.open("/rigsetup");
  if (dir && dir.isDirectory()) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      if (!f.isDirectory()) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\""; json += _sdJsonEsc(_stBaseName(String(f.name())));
        json += "\",\"size\":"; json += (uint32_t)f.size(); json += "}";
      }
      f.close();
    }
  }
  if (dir) dir.close();
  json += "]}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}


// ============================================================
//  WEB SERVER  START / STOP
// ============================================================
static bool _webServerRunning = false;

static bool _routesRegistered = false;

void startWebServer() {
  if (_webServerRunning) return;

  wsServer.begin();
  wsServer.onEvent(webSocketEvent);

  // server.stop() keeps the handler list, so register routes only once.
  // Re-registering on every AP/STA restart duplicated every handler and
  // leaked heap each cycle.
  if (_routesRegistered) {
    server.begin();
    _webServerRunning = true;
    return;
  }
  _routesRegistered = true;

  // ── Pages ──────────────────────────────────────────────────
  server.on("/",            handleRoot);
  server.on("/index.html",  handleRoot);   // Chrome may auto-append /index.html
  server.on("/networks",    handleNetworks);
  server.on("/installonpi", handleInstallPiPage);
  server.on("/info",        handleSysInfoPage);
  server.on("/logo.png",    handleLogo);

  // ── API ────────────────────────────────────────────────────
  server.on("/connect",   handleConnect);
  server.on("/status",    handleStatus);
  server.on("/api/info",  handleApiInfo);
  server.on("/addwifi",   HTTP_POST, handleAddWifi);
  server.on("/api/installonpi", HTTP_POST, handleApiInstallOnPi);
  server.on("/api/installcfg",    HTTP_GET,  handleApiInstallCfg);
  server.on("/api/installstatus", HTTP_GET,  handleApiInstallStatus);

  // ── NVS profile manager ────────────────────────────────────
  server.on("/profiles",          handleProfiles);
  server.on("/api/nvs",           handleApiNvs);
  server.on("/api/piprofiles",    handleApiPiProfiles);
  server.on("/api/piconns",       handleApiPiConns);
  server.on("/api/reqpiprofiles", HTTP_POST, handleApiReqPiProfiles);
  server.on("/api/nvssave",       HTTP_POST, handleApiNvsSave);
  server.on("/api/nvsdel",        HTTP_POST, handleApiNvsDel);
  server.on("/api/nvsconnect",    HTTP_POST, handleApiNvsConnect);
  server.on("/api/espconnect",    HTTP_POST, handleApiEspConnect);
  server.on("/api/espdisconnect", HTTP_POST, handleApiEspDisconnect);
  server.on("/api/espstatus",     HTTP_GET,  handleApiEspStatus);
  server.on("/api/espscan",       HTTP_POST, handleApiEspScanStart);
  server.on("/api/espscan",       HTTP_GET,  handleApiEspScanGet);

  // ── Quick buttons ──────────────────────────────────────────
  server.on("/api/quickbtns",       HTTP_GET,  handleGetQuickBtns);
  server.on("/api/quickbtns/save",  HTTP_POST, handleSaveQuickBtn);

  // ── Terminal ───────────────────────────────────────────────
  server.on("/terminal", []() {
    server.send_P(200, "text/html", TERMINAL_HTML);
  });
  server.on("/xterm.js", []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "max-age=86400");
    server.send_P(200, "application/javascript",
                  (const char *)XTERM_JS_GZ, XTERM_JS_GZ_LEN);
  });
  server.on("/xterm.css", []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "max-age=86400");
    server.send_P(200, "text/css",
                  (const char *)XTERM_CSS_GZ, XTERM_CSS_GZ_LEN);
  });

  // ── Macro / Menu GUI editor ────────────────────────────────
  server.on("/macros", HTTP_GET, []() {
    server.send_P(200, "text/html", MACROS_HTML);
  });

  // ── Storage page + SD / storage / RIGSETUP API ─────────────
  server.on("/storage", HTTP_GET, []() {
    server.send_P(200, "text/html", STORAGE_HTML);
  });
  server.on("/api/sd/status",        HTTP_GET,  handleSdStatus);
  server.on("/api/sd/share/start",   HTTP_POST, handleSdShareStart);
  server.on("/api/sd/share/stop",    HTTP_POST, handleSdShareStop);
  server.on("/api/sd/boot",          HTTP_POST, handleSdBoot);
  server.on("/api/storage/ffat",     HTTP_GET,  handleStorageListFfat);
  server.on("/api/storage/sd",       HTTP_GET,  handleStorageListSd);
  server.on("/api/storage/delete",   HTTP_POST, handleStorageDelete);
  server.on("/api/storage/copy",     HTTP_POST, handleStorageCopy);
  server.on("/api/storage/upload",   HTTP_POST, handleStorageUploadDone, handleStorageUpload);
  server.on("/api/storage/download", HTTP_GET,  handleStorageDownload);
  server.on("/api/rigsetup/start",      HTTP_POST, handleRigsetupStart);
  server.on("/api/rigsetup/stop",       HTTP_POST, handleRigsetupStop);
  server.on("/api/rigsetup/status",     HTTP_GET,  handleRigsetupStatus);
  server.on("/api/rigsetup/force-copy", HTTP_POST, handleRigsetupForceCopy);

  // AP profiles
  server.on("/ap-profiles",        HTTP_GET,  handleApProfilesPage);
  server.on("/api/approfiles",     HTTP_GET,  handleApiGetApProfiles);
  server.on("/api/approfile/save", HTTP_POST, handleApiSaveApProfile);
  server.on("/api/approfile/getpass", HTTP_GET,  handleApiGetApPass);
  server.on("/api/apstart",        HTTP_POST, handleApiApStart);
  server.on("/api/apstatus",       HTTP_GET,  handleApiApStatus);

  // ── OTA ────────────────────────────────────────────────────
  server.on("/update",           HTTP_GET,  handleOtaPage);
  server.on("/api/otaprogress",  HTTP_GET,  handleOtaProgress);
  server.on("/update", HTTP_POST,
    handleOtaDone,
    handleOtaUpload
  );

  // ── LittleFS file manager ──────────────────────────────────
  server.on("/api/files",     HTTP_GET,  handleApiFilesList);
  server.on("/api/files/del", HTTP_POST, handleFileDelete);
  server.on("/upload",        HTTP_POST, handleFileUploadDone, handleFileUpload);

  static const char *hdrs[] = { "Content-Length" };
  server.collectHeaders(hdrs, 1);

  // Serve files from LittleFS under /files/<name>; fall back to 404.
  server.onNotFound([]() {
    String uri = server.uri();
    if (uri.startsWith("/files/")) {
      handleFileServe();
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });

  server.begin();

  _webServerRunning = true;
}

void webServerStop() {
  if (!_webServerRunning) return;
  wsServer.close();
  memset(termSlotClient, -1, sizeof(termSlotClient));
  server.stop();
  _webServerRunning = false;
}


// ============================================================
//  ACCESS POINT  START / STOP
// ============================================================
// ── AP profile HTTP handlers ──────────────────────────────────

// Escape a string for embedding inside a JSON string literal.
static String _apJsonEsc(const char* in) {
  String out;
  for (const char* p = in; *p; p++) {
    char c = *p;
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
    else out += c;
  }
  return out;
}

// GET /ap-profiles — AP profile editor page
void handleApProfilesPage() {
  server.send_P(200, "text/html", AP_PROFILES_HTML);
}

// GET /api/approfiles — JSON array of all 4 AP profiles + subnet labels
void handleApiGetApProfiles() {
  String json = "[";
  for (int i = 0; i < MAX_AP_PROFILES; i++) {
    if (i) json += ",";
    json += "{\"slot\":" + String(i);
    json += ",\"name\":\"" + _apJsonEsc(apProfiles[i].name) + "\"";
    json += ",\"ssid\":\"" + _apJsonEsc(apProfiles[i].ssid) + "\"";
    json += ",\"pass\":\"********\"";   // never send the real password
    json += ",\"subnet\":" + String(apProfiles[i].subnet);
    json += ",\"subnetLabel\":\"" + String(AP_SUBNET_LABELS[apProfiles[i].subnet]) + "\"";
    json += ",\"nat\":"    + String(apProfiles[i].nat ? "true" : "false");
    json += ",\"valid\":"  + String(apProfiles[i].valid ? "true" : "false");
    json += ",\"active\":" + String((apRunning && activeApProfile == i) ? "true" : "false");
    json += ",\"onBoot\":"     + String(apProfiles[i].onBoot ? "true" : "false");
    json += ",\"hideOnBoot\":" + String(apProfiles[i].hideOnBoot ? "true" : "false");
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// GET /api/approfile/getpass?slot=N — return stored WPA2 password for Reveal button.
void handleApiGetApPass() {
  if (!server.hasArg("slot")) { server.send(400,"text/plain","ERR: missing slot"); return; }
  int slot = server.arg("slot").toInt();
  if (slot < 0 || slot >= MAX_AP_PROFILES) { server.send(400,"text/plain","ERR: invalid slot"); return; }
  // Escape so passwords containing " or \ still yield valid JSON
  String json = "{\"pass\":\"" + _apJsonEsc(apProfiles[slot].pass) + "\"}";
  server.sendHeader("Cache-Control","no-cache");
  server.send(200,"application/json",json);
}

// POST /api/approfile/save — save one AP profile slot
void handleApiSaveApProfile() {
  if (!server.hasArg("slot")) { server.send(400, "text/plain", "ERR: missing slot"); return; }
  int slot = server.arg("slot").toInt();
  if (slot < 0 || slot >= MAX_AP_PROFILES) { server.send(400, "text/plain", "ERR: invalid slot"); return; }

  if (server.hasArg("ssid")) {
    String ssid = server.arg("ssid");
    if (ssid.length() < 1 || ssid.length() > 32) { server.send(400, "text/plain", "ERR: SSID must be 1-32 chars"); return; }
  }
  bool newPass = server.hasArg("pass") && server.arg("pass").length() > 0 && server.arg("pass") != "********";
  if (newPass) {
    String pw = server.arg("pass");
    if (pw.length() < 8 || pw.length() > 63) { server.send(400, "text/plain", "ERR: password must be 8-63 chars"); return; }
  }

  if (server.hasArg("name")) strlcpy(apProfiles[slot].name, server.arg("name").c_str(), sizeof(apProfiles[slot].name));
  if (server.hasArg("ssid")) strlcpy(apProfiles[slot].ssid, server.arg("ssid").c_str(), sizeof(apProfiles[slot].ssid));
  if (newPass)               strlcpy(apProfiles[slot].pass, server.arg("pass").c_str(), sizeof(apProfiles[slot].pass));
  if (server.hasArg("subnet")) {
    int sub = server.arg("subnet").toInt();
    apProfiles[slot].subnet = (sub >= 0 && sub < MAX_AP_PROFILES) ? sub : 0;
  }
  if (server.hasArg("nat")) apProfiles[slot].nat = (server.arg("nat") == "1" || server.arg("nat") == "true");
  if (server.hasArg("hideOnBoot"))
    apProfiles[slot].hideOnBoot = (server.arg("hideOnBoot") == "1" || server.arg("hideOnBoot") == "true");
  if (server.hasArg("onBoot")) {
    bool ob = (server.arg("onBoot") == "1" || server.arg("onBoot") == "true");
    apProfiles[slot].onBoot = ob;
    if (ob) {
      // Only one profile may start on boot: clear the flag elsewhere
      for (int i = 0; i < MAX_AP_PROFILES; i++) {
        if (i != slot && apProfiles[i].onBoot) { apProfiles[i].onBoot = false; saveApProfile(i); }
      }
    }
  }
  saveApProfile(slot);
  server.send(200, "text/plain", "Saved AP profile " + String(slot + 1));
  if (currentState == STATE_AP_SELECT) updateContentArea();   // refresh names on display
}

// POST /api/apstart — request AP start (processed in loop() via pendingApStart)
void handleApiApStart() {
  if (!server.hasArg("slot")) { server.send(400, "text/plain", "ERR: missing slot"); return; }
  int slot = server.arg("slot").toInt();
  if (slot < 0 || slot >= MAX_AP_PROFILES) { server.send(400, "text/plain", "ERR: invalid slot"); return; }
  pendingApStart = slot;
  server.send(200, "text/plain", "Starting AP with profile " + String(slot + 1));
}

bool        apNatActive()  { return _natActive; }
const char* apNatStatus()  { return _natStatus; }

// True when the STA side really has an uplink (associated + DHCP address).
static bool _staUplink() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress((uint32_t)0);
}

// Enable/disable NAPT on the AP netif via the official esp_netif API
// (WiFi.AP.enableNAPT -> esp_netif_napt_enable). Unlike the raw
// ip_napt_enable(addr) call this binds to the netif itself, so it does not
// depend on the AP address already being applied to lwIP.
static bool _apSetNapt(bool enable) {
  if (enable == _natActive) return true;
  bool ok = WiFi.AP.enableNAPT(enable);
  if (ok) _natActive = enable;
  Serial.printf("[NAT] %s NAPT on AP %s: %s\n", enable ? "enable" : "disable",
                WiFi.softAPIP().toString().c_str(), ok ? "OK" : "FAILED");
  return ok;
}

// Called from loop(). Keeps NAPT in sync with the STA uplink so NAT also
// works when the ESP (re)connects to Wi-Fi after the AP was started, and
// reports a clear status when it can't work.
static void _apNatUpdate();
void apNatService() {
  static unsigned long lastCheck = 0;
  if (!apRunning && !_natActive) return;
  if (millis() - lastCheck < 2000) return;
  lastCheck = millis();
  char prev[sizeof(_natStatus)];
  strlcpy(prev, _natStatus, sizeof(prev));
  _apNatUpdate();
  if (strcmp(prev, _natStatus) != 0 && currentState == STATE_AP_ACTIVE) updateContentArea();
}

static void _apNatUpdate() {

  if (!_natWanted) { strlcpy(_natStatus, "off", sizeof(_natStatus)); return; }
  if (!_staUplink()) {
    if (_natActive) _apSetNapt(false);
    strlcpy(_natStatus, "no uplink", sizeof(_natStatus));
    return;
  }
  if (!_natActive) {
    if (_apSetNapt(true)) strlcpy(_natStatus, "on", sizeof(_natStatus));
    else                  strlcpy(_natStatus, "NAPT failed", sizeof(_natStatus));
  } else {
    strlcpy(_natStatus, "on", sizeof(_natStatus));
  }
}


// ============================================================
//  ACCESS POINT  START / STOP
// ============================================================
void startAP(int profileSlot) {
  if (profileSlot < 0 || profileSlot >= MAX_AP_PROFILES) profileSlot = 0;
  activeApProfile = profileSlot;
  apComputeNetwork(profileSlot);    // sets dynApIp, dynApGw, dynApSn
  const ApProfile& prof = apProfiles[profileSlot];

  // Restarting with another profile: drop NAPT from the previous AP first
  if (_natActive) _apSetNapt(false);

  bool uplink = espConnected && _staUplink();
  // Keep STA enabled while connected OR while a connect is in progress,
  // otherwise starting the AP would silently kill the STA attempt.
  WiFi.mode((espConnected || espConnecting) ? WIFI_AP_STA : WIFI_AP);
  WiFi.setSleep(false);   // re-assert: STA modem sleep throttles NAT traffic

  // The ESP32-S3 has one radio, so AP and STA must share a channel.
  // Start the AP directly on the STA channel instead of the default
  // channel 1; otherwise the driver moves the AP afterwards and clients
  // briefly lose the AP.
  int apChannel = uplink ? WiFi.channel() : 1;
  if (apChannel < 1 || apChannel > 13) apChannel = 1;

  // DNS handed to AP clients via DHCP.
  // When NAT is on, always use a public resolver (Cloudflare 1.1.1.1).
  // The hotel/upstream DNS is a private address that is unreachable once
  // traffic is NAPT-translated, which is why only IP pings worked but not
  // hostname resolution. 1.1.1.1 and 8.8.8.8 are always routable via NAPT.
  _natWanted = prof.nat;
  _natDns = prof.nat ? IPAddress(1, 1, 1, 1) : IPAddress((uint32_t)0);
  WiFi.softAPConfig(dynApIp, dynApGw, dynApSn, IPAddress((uint32_t)0), _natDns);
  WiFi.softAP(prof.ssid, prof.pass, apChannel, 0 /*ssid_hidden*/, AP_MAX_CLIENTS);
  // 20 MHz AP: more robust on crowded 2.4 GHz (hotels, offices) and
  // avoids HT40 secondary-channel conflicts with the STA uplink.
  WiFi.softAPbandwidth(WIFI_BW_HT20);

  Serial.printf("[AP] profile %d '%s' ip=%s ch=%d nat=%d sta=%s uplink=%d dns=%s\n",
                profileSlot + 1, prof.ssid, WiFi.softAPIP().toString().c_str(),
                apChannel, prof.nat, WiFi.localIP().toString().c_str(), uplink,
                _natDns.toString().c_str());

  if (!prof.nat)      strlcpy(_natStatus, "off", sizeof(_natStatus));
  else if (!uplink)   strlcpy(_natStatus, "no uplink", sizeof(_natStatus));
  else if (_apSetNapt(true)) strlcpy(_natStatus, "on", sizeof(_natStatus));
  else                strlcpy(_natStatus, "NAPT failed", sizeof(_natStatus));

  startWebServer();
  Serial.println("CMD:START_AP");
  apRunning = true;
}

void stopAP() {
  if (_natActive) _apSetNapt(false);
  _natWanted = false;
  strlcpy(_natStatus, "off", sizeof(_natStatus));
  WiFi.softAPdisconnect(true);
  if (espConnected || espConnecting) {
    WiFi.mode(WIFI_STA);
  } else {
    if (_webServerRunning) {
      wsServer.close();
      memset(termSlotClient, -1, sizeof(termSlotClient));
      server.stop();
      _webServerRunning = false;
    }
    WiFi.mode(WIFI_OFF);
  }
  Serial.println("CMD:STOP_AP");
  apRunning = false;
}

// GET /api/apstatus — AP / NAT / uplink diagnostics (for troubleshooting NAT)
void handleApiApStatus() {
  // Optional upstream DNS test from the ESP itself: /api/apstatus?test=1
  String dnsTest = "";
  if (server.hasArg("test") && _staUplink()) {
    IPAddress r;
    dnsTest = WiFi.hostByName("google.com", r) == 1 ? r.toString() : String("FAIL");
  }
  String j = "{";
  j += "\"apActive\":"   + String(apRunning ? "true" : "false");
  j += ",\"profile\":"   + String(activeApProfile + 1);
  j += ",\"apIp\":\""    + WiFi.softAPIP().toString() + "\"";
  j += ",\"clients\":"   + String((int)WiFi.softAPgetStationNum());
  j += ",\"maxClients\":" + String(AP_MAX_CLIENTS);
  // Per-client list: MAC from the Wi-Fi driver, IP from the DHCP server
  j += ",\"clientList\":[";
  if (apRunning) {
    static wifi_sta_list_t        wsl;   // static: keep ~0.5 KB off the stack
    static wifi_sta_mac_ip_list_t ipl;
    memset(&wsl, 0, sizeof(wsl));
    memset(&ipl, 0, sizeof(ipl));
    bool haveIp = false;
    if (esp_wifi_ap_get_sta_list(&wsl) == ESP_OK)
      haveIp = (esp_wifi_ap_get_sta_list_with_ip(&wsl, &ipl) == ESP_OK);
    for (int ci = 0; ci < wsl.num; ci++) {
      const uint8_t *mac = wsl.sta[ci].mac;
      char ipStr[16] = "";
      int  lastOctet = -1;
      if (haveIp && ipl.sta[ci].ip.addr != 0) {
        snprintf(ipStr, sizeof(ipStr), IPSTR, IP2STR(&ipl.sta[ci].ip));
        lastOctet = (int)esp_ip4_addr4(&ipl.sta[ci].ip);
      }
      const char *oui = _ouiLookup(mac);
      char macSuffix[10];
      snprintf(macSuffix, sizeof(macSuffix), "%02x:%02x:%02x", mac[3], mac[4], mac[5]);
      // host: sanitized to [A-Za-z0-9 ._-] in client_names.ino → no JSON escaping needed
      const char *host = clientNameFor(mac);
      char entry[200];
      snprintf(entry, sizeof(entry),
               "%s{\"ip\":\"%s\",\"dot\":%d,\"label\":\"%s\",\"host\":\"%s\",\"hostSrc\":\"%s\","
               "\"vendor\":\"%s\",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",
               ci ? "," : "", ipStr, lastOctet,
               host ? host : (oui ? oui : macSuffix),
               host ? host : "", clientNameSrc(mac), oui ? oui : "",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      j += entry;
    }
  }
  j += "]";
  j += ",\"dhcpNamesSeen\":" + String(clientNameDhcpSeen());   // DHCP requests with a host name
  j += ",\"natWanted\":" + String(_natWanted ? "true" : "false");
  j += ",\"natActive\":" + String(_natActive ? "true" : "false");
  j += ",\"natStatus\":\"" + String(_natStatus) + "\"";
  j += ",\"clientDns\":\"" + _natDns.toString() + "\"";
  j += ",\"staConnected\":" + String(_staUplink() ? "true" : "false");
  j += ",\"staSsid\":\""  + _apJsonEsc(WiFi.SSID().c_str()) + "\"";
  j += ",\"staIp\":\""    + WiFi.localIP().toString() + "\"";
  j += ",\"staGw\":\""    + WiFi.gatewayIP().toString() + "\"";
  j += ",\"staDns\":\""   + WiFi.dnsIP(0).toString() + "\"";
  j += ",\"staRssi\":"    + String(WiFi.RSSI());

  // Radio / performance diagnostics
  uint8_t prim = 0; wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&prim, &sec);
  wifi_ps_type_t ps = WIFI_PS_NONE;
  esp_wifi_get_ps(&ps);
  wifi_bandwidth_t apBw  = WIFI_BW_HT20;
  wifi_bandwidth_t staBw = WIFI_BW_HT20;
  esp_wifi_get_bandwidth(WIFI_IF_AP,  &apBw);
  esp_wifi_get_bandwidth(WIFI_IF_STA, &staBw);
  j += ",\"channel\":"    + String(prim);
  j += ",\"apBw\":"       + String(apBw  == WIFI_BW_HT40 ? 40 : 20);
  j += ",\"staBw\":"      + String(staBw == WIFI_BW_HT40 ? 40 : 20);
  j += ",\"powerSave\":"  + String(ps == WIFI_PS_NONE ? "false" : "true");
  j += ",\"txPowerDbm\":" + String((int)WiFi.getTxPower() / 4.0f, 1);
  j += ",\"staDrops\":"   + String(staDropCount);
  if (staDropCount) {
    j += ",\"staLastReason\":\"" + String(WiFi.disconnectReasonName((wifi_err_reason_t)staLastReason)) + "\"";
    j += ",\"staLastDropAgoS\":" + String((millis() - staLastDropMs) / 1000UL);
  }
  j += ",\"loopMaxMs\":"  + String(max(loopMaxUs, loopMaxPrevUs) / 1000.0f, 1);
  j += ",\"heapFreeKb\":" + String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
  j += ",\"heapMinKb\":"  + String(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024);
  j += ",\"uptimeS\":"    + String(millis() / 1000UL);
  if (dnsTest.length()) j += ",\"dnsTest\":\"" + dnsTest + "\"";
  j += "}";
  server.send(200, "application/json", j);
}
