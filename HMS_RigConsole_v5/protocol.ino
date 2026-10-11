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
void processMsg(const String& msg) {

  // ACK — Pi confirms handshake; start the polling clock
  if (msg == "ACK") {
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
    if (currentState == STATE_STATUS) { drawHomeContent(); gfx->flush(); }
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
    if (currentState == STATE_STATUS || currentState == STATE_AP_ACTIVE) { drawHomeContent(); gfx->flush(); }
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
    return;
  }
  if (msg.startsWith("ERR:ADDWIFI_FULL:")) {
    hasError = true;
    snprintf(lastErrMsg, sizeof(lastErrMsg), "ERR: %s", msg.substring(17).c_str());
    if (currentState == STATE_STATUS || currentState == STATE_AP_ACTIVE) {
      drawHomeContent();
      gfx->flush();
    }
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

  // TERM:OUT:<base64> — PTY output from Pi → WebSocket client
  if (msg.startsWith("TERM:OUT:")) {
    if (termWsClient >= 0) {
      String b64 = msg.substring(9);
      wsServer.sendTXT((uint8_t)termWsClient, b64);
    }
    return;
  }
}


// ============================================================
//  WEBSOCKET EVENT  (terminal bridge)
// ============================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    termWsClient = (int)num;
    Serial.println("CMD:TERM_OPEN");
  } else if (type == WStype_DISCONNECTED) {
    if (termWsClient == (int)num) {
      termWsClient = -1;
      Serial.println("CMD:TERM_CLOSE");
    }
  } else if (type == WStype_TEXT) {
    String data = String((char*)payload);
    if (data.startsWith("CTL:")) {
      // Control message (e.g. CTL:RESIZE:cols:rows) — pass through as-is
      Serial.println(data);
    } else {
      // Base64-encoded keystrokes → TERM:IN:<base64>
      Serial.print("TERM:IN:");
      Serial.println(data);
    }
  }
}
