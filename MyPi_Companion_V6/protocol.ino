// ============================================================
//  protocol.ino — Serial protocol & WebSocket bridge
//  Handles all Pi ↔ ESP32 message parsing and routing.
// ============================================================

// ============================================================
//  SERIAL READ
// ============================================================
void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      serialBuf.trim();
      if (serialBuf.length() > 0) processMsg(serialBuf);
      serialBuf = "";
    } else if (c != '\r') {
      if (serialBuf.length() < 6144) serialBuf += c;
    }
  }
}

// ============================================================
//  PROFILE LIST PARSER
// ============================================================
void parseProfiles(const String& payload) {
  numDynProfiles = 0;
  if (payload == "NONE") return;
  int start = 0;
  while (start < (int)payload.length() && numDynProfiles < MAX_WIFI_PROFILES) {
    int    comma = payload.indexOf(',', start);
    String token = (comma < 0) ? payload.substring(start)
                                : payload.substring(start, comma);
    token.trim();
    if (token.length() > 0) {
      strlcpy(dynProfiles[numDynProfiles], token.c_str(), MAX_PROFILE_LEN);
      numDynProfiles++;
    }
    if (comma < 0) break;
    start = comma + 1;
  }
}

// ============================================================
//  MESSAGE DISPATCHER
// ============================================================
// Value for a STATUS:SYS key, or "" if the daemon did not send it
const char* piSysGet(const char *key) {
  for (int i = 0; i < piSysCount; i++)
    if (strcmp(piSys[i].key, key) == 0) return piSys[i].val;
  return "";
}

void processMsg(const String& msg) {

  // ACK — Pi confirms handshake; start the polling clock
  if (msg == "ACK") {
    // Report why we (re)booted once per boot — shows up in the daemon journal,
    // which is otherwise not listening yet when setup() runs.
    static bool resetReported = false;
    if (!resetReported) {
      resetReported = true;
      Serial.printf("boot: reset reason %d (1=power-on 3=sw 4=panic 5/6/7=watchdog 9=brownout)\n",
                    (int)esp_reset_reason());
    }
    piConnected   = true;
    lastStatusMs  = millis();
    lastPollMs    = 0;           // Trigger first CMD:GET_STATUS immediately
    if (currentState == STATE_STATUS) { drawHomeContent(); gfx->flush(); }
    return;
  }

  // PONG — legacy keepalive reply (ignored; polling replaces PING)
  if (msg == "PONG") return;

  // STATUS:SSID:<ssid>  — actual Wi-Fi SSID for display
  if (msg.startsWith("STATUS:SSID:")) {
    strlcpy(activeSSID, msg.substring(12).c_str(), MAX_PROFILE_LEN);
    lastStatusMs = millis();
    if (!piConnected) { piConnected = true; }
    drawStatusBar();
    gfx->flush();
    return;
  }

  // STATUS:CONN:<name>  — NetworkManager connection name used for CMD:CONNECT
  if (msg.startsWith("STATUS:CONN:")) {
    strlcpy(activeConn, msg.substring(12).c_str(), MAX_PROFILE_LEN);
    return;
  }

  // STATUS:IP:<ip>
  if (msg.startsWith("STATUS:IP:")) {
    strlcpy(activeIP, msg.substring(10).c_str(), sizeof(activeIP));
    lastStatusMs = millis();
    if (!piConnected) { piConnected = true; }
    drawStatusBar();
    gfx->flush();
    return;
  }

  // STATUS:PORT:<device>  — strip leading /dev/ to save display width
  if (msg.startsWith("STATUS:PORT:")) {
    String p = msg.substring(12);
    if (p.startsWith("/dev/")) p = p.substring(5);
    strlcpy(piPort, p.c_str(), sizeof(piPort));
    return;
  }

  // STATUS:IFACES:<name1>:<ip1>,<name2>:<ip2>,...  (or NONE)
  if (msg.startsWith("STATUS:IFACES:")) {
    String body = msg.substring(14);
    piIfaceCount = 0;
    if (body != "NONE") {
      int start = 0;
      while (start < (int)body.length() && piIfaceCount < MAX_PI_IFACES) {
        int comma  = body.indexOf(',', start);
        String tok = (comma < 0) ? body.substring(start)
                                 : body.substring(start, comma);
        tok.trim();
        int col = tok.lastIndexOf(':');   // last colon separates name from IP
        if (col > 0) {
          String nm = tok.substring(0, col);
          String ip = tok.substring(col + 1);
          nm.trim(); ip.trim();
          // Interface names never contain spaces; older daemons appended
          // the SSID ("wlan0 Ibis Style") — keep only the real name.
          int sp = nm.indexOf(' ');
          if (sp > 0) nm = nm.substring(0, sp);
          if (nm.length() > 0 && ip.length() > 0) {
            strlcpy(piIfaces[piIfaceCount].name, nm.c_str(), PI_IFACE_NAME_LEN);
            strlcpy(piIfaces[piIfaceCount].ip,   ip.c_str(), sizeof(piIfaces[0].ip));
            piIfaceCount++;
          }
        }
        if (comma < 0) break;
        start = comma + 1;
      }
    }
    // Keep the scroll position across 5 s polls so every interface gets shown
    if (piIfaceIdx >= piIfaceCount) piIfaceIdx = 0;
    lastStatusMs = millis();
    if (!piConnected) piConnected = true;
    if (currentState == STATE_STATUS) { drawStatusBar(); gfx->flush(); }
    return;
  }

  // STATUS:SYS:<k>=<v>|<k>=<v>|...  — Pi system info, replaces the whole table
  if (msg.startsWith("STATUS:SYS:")) {
    const char *p = msg.c_str() + 11;
    piSysCount = 0;
    while (*p && piSysCount < MAX_PI_SYS) {
      const char *end = strchr(p, '|');
      if (!end) end = p + strlen(p);
      const char *eq = (const char *)memchr(p, '=', end - p);
      if (eq && eq > p) {
        PiSysKV &kv = piSys[piSysCount];
        size_t kl = min((size_t)(eq - p), (size_t)PI_SYS_KEY_LEN - 1);
        size_t vl = min((size_t)(end - eq - 1), (size_t)PI_SYS_VAL_LEN - 1);
        memcpy(kv.key, p, kl);       kv.key[kl] = 0;
        memcpy(kv.val, eq + 1, vl);  kv.val[vl] = 0;
        piSysCount++;
      }
      p = *end ? end + 1 : end;
    }
    piSysMs = millis();
    if (currentState == STATE_ADV_INFO && advInfoPage == 1) updateContentArea();
    return;
  }

  // PROFILES:<p1>,<p2>,...
  if (msg.startsWith("PROFILES:")) {
    parseProfiles(msg.substring(9));
    if (menuIndex >= numDynProfiles + 1) menuIndex = 0;
    if (currentState == STATE_MENU) updateContentArea();
    return;
  }

  // OK:CONNECT:<profile>
  if (msg.startsWith("OK:CONNECT:")) {
    hasError = false;
    return;
  }

  // ERR:CONNECT:<profile>
  if (msg.startsWith("ERR:CONNECT:")) {
    hasError = true;
    snprintf(lastErrMsg, sizeof(lastErrMsg), "ERR: %s", msg.substring(12).c_str());
    toastError(lastErrMsg);
    return;
  }

  // OK:ADDWIFI:<ssid>
  if (msg.startsWith("OK:ADDWIFI:")) {
    hasError = false;
    return;
  }

  // ERR:ADDWIFI:<ssid>
  if (msg.startsWith("ERR:ADDWIFI:")) {
    hasError = true;
    snprintf(lastErrMsg, sizeof(lastErrMsg), "Failed: %s", msg.substring(12).c_str());
    toastError(lastErrMsg);
    return;
  }

  // PROFILES_FULL:ssid1|pass1,ssid2|pass2,...
  if (msg.startsWith("PROFILES_FULL:")) {
    String payload = msg.substring(14);
    numPiProfiles = 0;
    if (payload != "NONE") {
      int start = 0;
      while (start < (int)payload.length() && numPiProfiles < MAX_PI_PROFILES) {
        int    comma = payload.indexOf(',', start);
        String token = (comma < 0) ? payload.substring(start)
                                   : payload.substring(start, comma);
        token.trim();
        int pipe = token.indexOf('|');
        if (pipe >= 0) {
          strlcpy(piProfiles[numPiProfiles].ssid,
                  token.substring(0, pipe).c_str(), 33);
          strlcpy(piProfiles[numPiProfiles].pass,
                  token.substring(pipe + 1).c_str(), 65);
          numPiProfiles++;
        }
        if (comma < 0) break;
        start = comma + 1;
      }
    }
    piProfilesFresh = true;
    return;
  }

  // OK:ADDWIFI_FULL / ERR:ADDWIFI_FULL
  if (msg.startsWith("OK:ADDWIFI_FULL:")) {
    hasError = false;
    char buf[40];
    snprintf(buf, sizeof(buf), "Tillagd: %s", msg.substring(16).c_str());
    toastInfo(buf);      // replaces the pending "Lägger till WiFi..." toast
    return;
  }
  if (msg.startsWith("ERR:ADDWIFI_FULL:")) {
    hasError = true;
    snprintf(lastErrMsg, sizeof(lastErrMsg), "ERR: %s", msg.substring(17).c_str());
    toastError(lastErrMsg);   // replaces the pending "Lägger till WiFi..." toast
    return;
  }

  // SCAN_RESULT:<ssid>:<signal>:<freq>
  //   Pi sends one line per discovered network during a CMD:SCAN_WIFI run.
  //   Deduplicated in the daemon; we just append (up to MAX_SCAN_RESULTS).
  if (msg.startsWith("SCAN_RESULT:")) {
    if (piScanCount < MAX_SCAN_RESULTS) {
      // Format: SCAN_RESULT:<ssid>:<signal>:<freq>
      // SSID may itself contain colons, signal and freq never do — split from right.
      String body = msg.substring(12);  // after "SCAN_RESULT:"
      int lastColon2 = body.lastIndexOf(':');
      if (lastColon2 > 0) {
        int lastColon1 = body.lastIndexOf(':', lastColon2 - 1);
        if (lastColon1 > 0) {
          String ssid = body.substring(0, lastColon1);
          ssid.trim();
          if (ssid.length() > 0 && ssid.length() < 33) {
            strlcpy(piScanSSIDs[piScanCount], ssid.c_str(), 33);
            piScanCount++;
          }
        }
      }
    }
    return;
  }

  // SCAN_DONE — Pi finished the nmcli scan
  if (msg == "SCAN_DONE") {
    piScanBusy = false;
    if (currentState == STATE_PI_SCAN) {
      updateContentArea();   // refresh to show results (replaces "Scanning...")
    }
    return;
  }

  // OK:MSC_EJECT / ERR:MSC_EJECT:<reason> — Pi flushed + unmounted the MSC volume
  if (msg == "OK:MSC_EJECT")          { mscOnHostEjectReply(true, "");               return; }
  if (msg.startsWith("ERR:MSC_EJECT")) { mscOnHostEjectReply(false, msg.substring(14)); return; }

  // TERM:OUT:<slot>:<base64> — PTY output from Pi -> correct WebSocket client
  if (msg.startsWith("TERM:OUT:")) {
    int p = 9;
    int colon = msg.indexOf(':', p);
    if (colon < 0) return;
    int slot = msg.substring(p, colon).toInt();
    if (slot >= 0 && slot < MAX_TERM_SESSIONS && termSlotClient[slot] >= 0) {
      String b64 = msg.substring(colon + 1);   // sendTXT needs a non-const String&
      wsServer.sendTXT((uint8_t)termSlotClient[slot], b64);
    }
    return;
  }
}


// ============================================================
//  WEBSOCKET EVENT  (terminal bridge)
// ============================================================
// ── Slot helpers ──────────────────────────────────────────
static int8_t _slotForClient(uint8_t num) {
  for (int i = 0; i < MAX_TERM_SESSIONS; i++)
    if (termSlotClient[i] == (int8_t)num) return (int8_t)i;
  return -1;
}
static int8_t _freeSlot() {
  for (int i = 0; i < MAX_TERM_SESSIONS; i++)
    if (termSlotClient[i] == -1) return (int8_t)i;
  return -1;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    int8_t slot = _freeSlot();
    if (slot < 0) {
      // All sessions taken — reject and close
      wsServer.sendTXT(num, "ERR:SESSION_FULL");
      wsServer.disconnect(num);
      return;
    }
    termSlotClient[slot] = (int8_t)num;
    String sess = String("SESS:") + slot;
    wsServer.sendTXT(num, sess);
    Serial.println(String("CMD:TERM_OPEN:") + slot);

  } else if (type == WStype_DISCONNECTED) {
    int8_t slot = _slotForClient(num);
    if (slot >= 0) {
      termSlotClient[slot] = -1;
      Serial.println(String("CMD:TERM_CLOSE:") + slot);
    }

  } else if (type == WStype_TEXT) {
    int8_t slot = _slotForClient(num);
    if (slot < 0) return;
    String data = String((char*)payload);
    if (data.startsWith("CTL:RESIZE:")) {
      // CTL:RESIZE:cols:rows -> CTL:RESIZE:<slot>:cols:rows
      Serial.println(String("CTL:RESIZE:") + slot + ":" + data.substring(11));
    } else {
      // Base64 keystrokes -> TERM:IN:<slot>:<b64>
      Serial.println(String("TERM:IN:") + slot + ":" + data);
    }
  }
}
