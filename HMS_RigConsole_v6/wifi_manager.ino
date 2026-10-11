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
//    wifiConnectSlot(int)     — connect to one NVS slot (manual)
//    wifiDisconnect()         — drop current STA connection
//    wifiScanStart()          — begin async SSID scan
//    wifiScanResults(...)     — retrieve results (call after scan)
//    espSSID[33]              — current ESP SSID (or "---")
//    espIP[24]                — current ESP IP   (or "---")
//    espConnected             — bool
// ============================================================

#include <ESPmDNS.h>

// espSSID, espIP, espConnected, pendingPiInstall, espMenuIndex
// are declared in HMS_RigConsole_v4.ino (the main .ino is
// concatenated first, so all .ino files can see them).

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
  WiFi.setSleep(false);   // keep radio awake for low-latency serial

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
      _onConnected();
      return;
    }
  }

  WiFi.scanDelete();
  // No connection — remain offline, WiFi stays in STA mode (idle)
}

// -----------------------------------------------------------
//  wifiConnectSlot()
//  Manual connect from the touch menu.  Disconnects first if
//  already connected.  Updates display globals.
// -----------------------------------------------------------
bool wifiConnectSlot(int slot) {
  if (slot < 0 || slot >= MAX_NVS_SLOTS) return false;
  if (!nvsProfiles[slot].valid) return false;

  // Tear down any existing STA connection / web server
  wifiDisconnect();
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (_connectSlot(slot)) {
    _onConnected();
    return true;
  }
  return false;
}

// -----------------------------------------------------------
//  wifiDisconnect()
//  Stops web server, mDNS, and disconnects STA.
// -----------------------------------------------------------
void wifiDisconnect() {
  if (espConnected) {
    webServerStop();   // stop server + wsServer + reset _webServerRunning
    MDNS.end();
    espConnected = false;
    strlcpy(espSSID, "---", sizeof(espSSID));
    strlcpy(espIP,   "---", sizeof(espIP));
  }
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
