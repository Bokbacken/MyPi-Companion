// ============================================================
//  web_server.ino — HTTP handlers, web server, AP setup/teardown, web OTA
//
//  startWebServer() registers all routes and starts the server.
//  It is called by _onConnected() (STA mode) AND by startAP().
//  The _webServerRunning flag prevents double-registration.
//
//  Route list:
//    GET  /                   — dashboard (status + 3×2 navigation grid)
//    GET  /hms_logo.png       — HMS corner logo as PNG (served from PROGMEM)
//    GET  /networks           — Pi networks page (list + add new)
//    GET  /installonpi        — Install on Pi confirmation page
//    GET  /info               — System information page
//    GET  /connect            — connect Pi to named profile
//    GET  /status             — JSON status (Pi + ESP)
//    GET  /api/info           — JSON system diagnostics
//    POST /addwifi            — add Pi profile via USB serial
//    POST /api/installonpi    — set pendingPiInstall flag
//    GET  /profiles           — NVS profile manager page
//    GET  /api/nvs            — NVS slot JSON (shared pool)
//    GET  /api/piprofiles     — Pi-side profile list
//    POST /api/reqpiprofiles  — request fresh Pi profile list
//    POST /api/nvssave        — save NVS slot (ESP + Pi profile)
//    POST /api/nvsdel         — delete NVS slot
//    POST /api/nvsconnect     — tell Pi to connect NVS slot
//    GET  /terminal           — xterm.js terminal page
//    GET  /macros             — GUI macro & menu editor
//    GET  /update             — OTA firmware upload page
//    POST /update             — receive + flash firmware binary
//    GET  /api/files          — LittleFS file list (JSON)
//    POST /api/files/del      — delete a LittleFS file  (field: name)
//    POST /upload             — upload a file to LittleFS  (field: file)
//    GET  /files/<name>       — serve a file from LittleFS (via onNotFound)
// ============================================================

#include "hms_logo_web.h"   // HMS_LOGO_WEB_PNG / HMS_LOGO_WEB_PNG_LEN
#include "page_html.h"   // PAGE_TMPL, NETWORKS_HTML, INSTALLPI_HTML, SYSINFO_HTML, OTA_HTML
#include "macros_html.h" // MACROS_HTML — GUI macro/menu editor at /macros


// ============================================================
//  BASIC HTTP HANDLERS
// ============================================================

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

void handleHmsLogo() {
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, "image/png",
                (const char *)HMS_LOGO_WEB_PNG, HMS_LOGO_WEB_PNG_LEN);
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
  json += (currentState == STATE_AP_ACTIVE) ? "true" : "false";
  json += "}";
  server.send(200, "application/json", json);
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
  json += "}";
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

// POST /api/installonpi — triggers HID installer via loop()
void handleApiInstallOnPi() {
  pendingPiInstall = true;
  server.send(200, "text/plain", "Installation started via USB HID — watch the Pi display.");
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
//  OTA HANDLERS  (web-based, replaces ArduinoOTA)
// ============================================================
static size_t _otaTotal   = 0;
static size_t _otaWritten = 0;

void handleOtaPage() {
  server.send_P(200, "text/html", OTA_HTML);
}

void handleOtaUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    _otaTotal   = 0;
    _otaWritten = 0;
    drawOtaScreen(-1);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
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
      int pct = (int)((uint64_t)_otaWritten * 100 / _otaTotal);
      drawOtaScreen(constrain(pct, 0, 99));
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      drawOtaScreen(101);
    } else {
      char errBuf[40];
      snprintf(errBuf, sizeof(errBuf), "End failed: %s",
               Update.errorString());
      drawOtaError(errBuf);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    drawOtaError("Aborted");
  }
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
//  WEB SERVER  START / STOP
// ============================================================
static bool _webServerRunning = false;

void startWebServer() {
  if (_webServerRunning) return;

  wsServer.begin();
  wsServer.onEvent(webSocketEvent);

  // ── Pages ──────────────────────────────────────────────────
  server.on("/",            handleRoot);
  server.on("/networks",    handleNetworks);
  server.on("/installonpi", handleInstallPiPage);
  server.on("/info",        handleSysInfoPage);
  server.on("/hms_logo.png", handleHmsLogo);

  // ── API ────────────────────────────────────────────────────
  server.on("/connect",   handleConnect);
  server.on("/status",    handleStatus);
  server.on("/api/info",  handleApiInfo);
  server.on("/addwifi",   HTTP_POST, handleAddWifi);
  server.on("/api/installonpi", HTTP_POST, handleApiInstallOnPi);

  // ── NVS profile manager ────────────────────────────────────
  server.on("/profiles",          handleProfiles);
  server.on("/api/nvs",           handleApiNvs);
  server.on("/api/piprofiles",    handleApiPiProfiles);
  server.on("/api/reqpiprofiles", HTTP_POST, handleApiReqPiProfiles);
  server.on("/api/nvssave",       HTTP_POST, handleApiNvsSave);
  server.on("/api/nvsdel",        HTTP_POST, handleApiNvsDel);
  server.on("/api/nvsconnect",    HTTP_POST, handleApiNvsConnect);

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

  // ── OTA ────────────────────────────────────────────────────
  server.on("/update", HTTP_GET,  handleOtaPage);
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
  termWsClient = -1;
  server.stop();
  _webServerRunning = false;
}


// ============================================================
//  ACCESS POINT  START / STOP
// ============================================================
void startAP() {
  if (espConnected) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_AP);
  }
  WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
  WiFi.softAP(AP_SSID, AP_PASS);
  startWebServer();
  Serial.println("CMD:START_AP");
}

void stopAP() {
  WiFi.softAPdisconnect(true);
  if (espConnected) {
    WiFi.mode(WIFI_STA);
  } else {
    if (_webServerRunning) {
      wsServer.close();
      termWsClient = -1;
      server.stop();
      _webServerRunning = false;
    }
    WiFi.mode(WIFI_OFF);
  }
  Serial.println("CMD:STOP_AP");
}
