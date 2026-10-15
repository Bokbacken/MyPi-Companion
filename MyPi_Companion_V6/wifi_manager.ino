// ============================================================
//  wifi_manager.ino — ESP32 Wi-Fi connection management
//
//  Philosophy:
//    • NVS slots 0-9 are tried in order at boot (slot 0 = highest
//      priority).  Only slots whose SSID is visible in a scan are
//      attempted — avoids long timeouts for absent networks.
//    • If no slot connects → stay offline (no auto-AP).
//    • AP is started only explicitly from the Advanced menu.
//    • When connected in STA mode, the web server + WebSocket
//      bridge start immediately on the assigned IP.
//    • mDNS name: rigconsole.local
//
//  Public API:
//    wifiAutoConnect()        — call once in setup()
//    wifiConnectSlot(int)     — start a non-blocking connect to one
//                               NVS slot (touch menu / web UI)
//    wifiService()            — call every loop(); finishes the
//                               connect started by wifiConnectSlot()
//    wifiDisconnect()         — drop current STA connection
//
//  AP coexistence: when the AP is running (apRunning), STA work is
//  done in WIFI_AP_STA mode and never tears down the AP or the web
//  server. Note the radio has one channel: when STA associates, the
//  AP follows the STA channel and AP clients may reconnect briefly.
//    wifiScanStart()          — begin async SSID scan
//    wifiScanResults(...)     — retrieve results (call after scan)
//    espSSID[33]              — current ESP SSID (or "---")
//    espIP[24]                — current ESP IP   (or "---")
//    espConnected             — bool
// ============================================================

#include <ESPmDNS.h>
#include <esp_wifi.h>   // esp_wifi_set_bandwidth — lock STA to HT20

// espSSID, espIP, espConnected, pendingPiInstall, espMenuIndex
// are declared in MyPi_Companion_V6.ino (the main .ino is
// concatenated first, so all .ino files can see them).

// -----------------------------------------------------------
//  STA link diagnostics (shown on /ap-profiles via /api/apstatus)
//  Written from the Arduino event task, read from loop().
//  No Serial output here: Serial is the Pi protocol channel and
//  printing from another task could interleave protocol lines.
// -----------------------------------------------------------
volatile uint32_t staDropCount   = 0;   // STA disconnects since boot
volatile uint8_t  staLastReason  = 0;   // wifi_err_reason_t of last drop
volatile uint32_t staLastDropMs  = 0;   // millis() of last drop

static void _onStaDisconnected(arduino_event_id_t, arduino_event_info_t info) {
  staDropCount++;
  staLastReason = info.wifi_sta_disconnected.reason;
  staLastDropMs = millis();
}

// Call once in setup(), before the first WiFi.mode().
void wifiDiagInit() {
  WiFi.onEvent(_onStaDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

// -----------------------------------------------------------
//  wifiLockGN()
//  Lock BOTH interfaces (STA and AP) to 802.11 g/n, no 11b rates.
//  Like HT20, this only takes effect when set while the radio is
//  stopped: esp_wifi_config_11b_rate() must be called before
//  esp_wifi_start(), and a running AP keeps its b/g/n beacons.
//  So: stop the radio, temporarily enable both interfaces (so the AP
//  can be configured even in STA-only mode), set protocol + 11b off,
//  restore the mode, start again. The config lives in the driver
//  until WiFi is deinitialised (WiFi.mode(WIFI_OFF)).
//  Call right after every WiFi.mode() that turns an interface on.
//  Cheap no-op when the enabled interfaces are already g/n, so a
//  restart (which drops STA/AP links) normally happens only at boot
//  or after WIFI_OFF, before anything is connected.
// -----------------------------------------------------------
static const uint8_t WIFI_GN = WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;

static bool _ifIsGN(wifi_interface_t ifx) {
  uint8_t bm = 0;
  return esp_wifi_get_protocol(ifx, &bm) == ESP_OK && bm == WIFI_GN;
}

void wifiLockGN() {
  wifi_mode_t m = WIFI_MODE_NULL;
  if (esp_wifi_get_mode(&m) != ESP_OK || m == WIFI_MODE_NULL) return;
  bool staOk = !(m & WIFI_MODE_STA) || _ifIsGN(WIFI_IF_STA);
  bool apOk  = !(m & WIFI_MODE_AP)  || _ifIsGN(WIFI_IF_AP);
  if (staOk && apOk) return;

  esp_err_t eStop = esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_err_t pSta = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_GN);
  esp_err_t pAp  = esp_wifi_set_protocol(WIFI_IF_AP,  WIFI_GN);
  esp_err_t rSta = esp_wifi_config_11b_rate(WIFI_IF_STA, true);
  esp_err_t rAp  = esp_wifi_config_11b_rate(WIFI_IF_AP,  true);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  esp_wifi_set_bandwidth(WIFI_IF_AP,  WIFI_BW_HT20);
  esp_wifi_set_mode(m);
  esp_err_t eStart = esp_wifi_start();
  WiFi.setSleep(false);

  uint8_t bs = 0, ba = 0;
  esp_wifi_get_protocol(WIFI_IF_STA, &bs);
  esp_wifi_get_protocol(WIFI_IF_AP,  &ba);
  Serial.printf("[WIFI] lock g/n (mode %d): stop=%s proto sta=%s ap=%s 11b_off sta=%s ap=%s start=%s -> STA %s, AP %s\n",
                (int)m, esp_err_to_name(eStop), esp_err_to_name(pSta), esp_err_to_name(pAp),
                esp_err_to_name(rSta), esp_err_to_name(rAp), esp_err_to_name(eStart),
                apProtocolStr(bs).c_str(), apProtocolStr(ba).c_str());
}

// -----------------------------------------------------------
//  Internal helpers
// -----------------------------------------------------------

// Try to connect to one slot; returns true on success.
static bool _connectSlot(int slot) {
  if (slot < 0 || slot >= MAX_NVS_SLOTS) return false;
  if (!nvsProfiles[slot].valid) return false;

  WiFi.begin(nvsProfiles[slot].ssid, nvsProfiles[slot].pass);

  unsigned long t0 = millis();
  while (millis() - t0 < WIFI_CONNECT_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(100);
  }
  WiFi.disconnect(true);
  return false;
}

// Start mDNS + web server after a successful STA connection.
static void _onConnected() {
  espConnected = true;
  strlcpy(espSSID, WiFi.SSID().c_str(), sizeof(espSSID));
  WiFi.localIP().toString().toCharArray(espIP, sizeof(espIP));

  // mDNS — reachable as  http://rigconsole.local
  if (MDNS.begin("rigconsole")) {
    MDNS.addService("http", "tcp", 80);
  }

  // Start HTTP server + WebSocket bridge (wsServer.begin/onEvent handled inside)
  startWebServer();

  Serial.print("WiFi connected: ");
  Serial.print(espSSID);
  Serial.print(" / ");
  Serial.println(espIP);
}

// -----------------------------------------------------------
//  wifiAutoConnect()
//  Called once from setup().  Scans visible networks, then tries
//  slots in order 0-9, skipping SSIDs not in the scan result.
// -----------------------------------------------------------
void wifiAutoConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  // Lock STA to 20 MHz before any connection attempt.
  // In AP_STA mode both interfaces share one radio; if STA negotiates
  // HT40 the whole radio runs at 40 MHz even when AP is set to HT20.
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  wifiLockGN();

  // Count valid slots — nothing to do if pool is empty
  int validCount = 0;
  for (int i = 0; i < MAX_NVS_SLOTS; i++)
    if (nvsProfiles[i].valid) validCount++;
  if (validCount == 0) return;

  // Reuse a pre-completed scan (started during splash) if available.
  // WiFi.scanComplete() returns WIFI_SCAN_RUNNING (-1) while busy,
  // WIFI_SCAN_FAILED (-2) on error, or the count when done.
  int found = WiFi.scanComplete();
  if (found < 0) {
    // No async scan done yet — run a blocking scan now
    found = WiFi.scanNetworks(false /*async*/, true /*show_hidden*/);
  }
  if (found < 0) found = 0;   // treat error as zero visible networks

  for (int slot = 0; slot < MAX_NVS_SLOTS; slot++) {
    if (!nvsProfiles[slot].valid) continue;

    // Check if SSID is visible in scan results
    bool visible = false;
    for (int n = 0; n < found; n++) {
      if (WiFi.SSID(n) == nvsProfiles[slot].ssid) { visible = true; break; }
    }
    if (!visible) continue;

    if (_connectSlot(slot)) {
      WiFi.scanDelete();   // free scan memory
      espActiveSlot = slot;
      _onConnected();
      return;
    }
  }

  WiFi.scanDelete();
  // No connection — remain offline, WiFi stays in STA mode (idle)
}

// -----------------------------------------------------------
//  wifiConnectSlot()
//  Manual connect (touch menu or web UI). Non-blocking: drops any
//  current STA link, starts WiFi.begin() and returns at once.
//  wifiService() completes or times out the attempt from loop(),
//  so the web server, AP and touch stay responsive meanwhile.
//  Returns false only for an invalid/empty slot.
// -----------------------------------------------------------
// espConnecting / espConnSlot / espConnResult / espConnTarget are
// declared in MyPi_Companion_V6.ino (visible to display + web code).
static unsigned long _connT0 = 0;

bool wifiConnectSlot(int slot) {
  if (slot < 0 || slot >= MAX_NVS_SLOTS) return false;
  if (!nvsProfiles[slot].valid) return false;

  // Drop any existing STA link. Keeps AP + web server if the AP runs.
  wifiDisconnect();
  delay(100);

  // AP_STA keeps the AP alive; plain STA otherwise.
  WiFi.mode(apRunning ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  wifiLockGN();   // no-op unless the STA interface was just (re)initialised
  WiFi.begin(nvsProfiles[slot].ssid, nvsProfiles[slot].pass);

  espConnecting = true;
  espConnSlot   = slot;
  espConnResult = 0;
  strlcpy(espConnTarget, nvsProfiles[slot].ssid, sizeof(espConnTarget));
  _connT0 = millis();
  Serial.printf("[WIFI] connecting to '%s' (slot %d, ap=%d)\n",
                espConnTarget, slot + 1, apRunning);
  return true;
}

// -----------------------------------------------------------
//  wifiService()
//  Called every loop(). Finishes a pending wifiConnectSlot().
// -----------------------------------------------------------
void wifiService() {
  if (!espConnecting) return;

  bool done = WiFi.status() == WL_CONNECTED &&
              WiFi.localIP() != IPAddress((uint32_t)0);
  if (done) {
    espConnecting = false;
    espConnResult = 1;
    espActiveSlot = espConnSlot;
    _onConnected();          // web server is a no-op if already running
  } else if (millis() - _connT0 >= WIFI_CONNECT_TIMEOUT_MS) {
    espConnecting = false;
    espConnResult = 2;
    // Disable STA only. In AP_STA mode the AP stays up; an idle STA
    // would keep scanning for the network and hop the shared radio
    // off the AP channel.
    WiFi.disconnect(true);
    Serial.printf("[WIFI] connect to '%s' failed\n", espConnTarget);
  } else {
    return;
  }
  drawStatusBar();
  gfx->flush();
  if (currentState == STATE_STATUS || currentState == STATE_AP_ACTIVE)
    updateContentArea();
}

// -----------------------------------------------------------
//  wifiDisconnect()
//  Disconnects STA. Stops web server + mDNS only when the AP is
//  not running (the AP side still needs them).
// -----------------------------------------------------------
void wifiDisconnect() {
  espConnecting = false;
  if (espConnected) {
    if (!apRunning) {
      webServerStop();   // stop server + wsServer + reset _webServerRunning
      MDNS.end();
    }
    espConnected  = false;
    espActiveSlot = -1;
    strlcpy(espSSID, "---", sizeof(espSSID));
    strlcpy(espIP,   "---", sizeof(espIP));
  }
  // disconnect(true) disables STA only: AP_STA -> AP, STA -> off.
  WiFi.disconnect(true);
}

// -----------------------------------------------------------
//  wifiScanStart()
//  Kick off an asynchronous scan.  Call wifiScanResults() to
//  retrieve results once complete (check WiFi.scanComplete()).
// -----------------------------------------------------------
void wifiScanStart() {
  WiFi.scanNetworks(true /*async*/, true /*show_hidden*/);
}

// -----------------------------------------------------------
//  wifiScanResults()
//  Fills `out` with up to `maxCount` SSID strings.
//  Returns number of networks found, or -1 if scan not done yet.
// -----------------------------------------------------------
int wifiScanResults(char out[][33], int maxCount) {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return -1;   // still in progress
  if (n <= 0) return 0;

  int count = (n < maxCount) ? n : maxCount;
  for (int i = 0; i < count; i++) {
    strlcpy(out[i], WiFi.SSID(i).c_str(), 33);
  }
  return count;
}
