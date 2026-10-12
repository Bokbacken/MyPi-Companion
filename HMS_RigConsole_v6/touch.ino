// ============================================================
//  touch.ino — Gesture detection, state machine, screen updates
//
//  Gesture detection:
//    Swipes register from anywhere on screen (dominant axis wins):
//      dx dominant → ACT_SWIPE_LEFT / ACT_SWIPE_RIGHT
//      dy dominant → ACT_SWIPE_UP   / ACT_SWIPE_DOWN
//      (threshold: SWIPE_MIN_PX = 22)
//
//    Taps only register when start y >= TOUCH_TOP (physical bar):
//      x < SCREEN_W/3        → ACT_TAP_LEFT
//      x < SCREEN_W*2/3      → ACT_TAP_MID
//      x >= SCREEN_W*2/3     → ACT_TAP_RIGHT
//      (max displacement: TAP_MAX_PX = 14)
//
//  STATE_ESP_KB dispatch:
//    SWIPE_LEFT  → kbRangePrev()    (previous range tab)
//    SWIPE_RIGHT → kbRangeNext()    (next range tab)
//    SWIPE_UP    → kbCharPrev()     (previous character)
//    SWIPE_DOWN  → kbCharNext()     (next character)
//    TAP_LEFT    → kbInsertCurrent() (insert highlighted char)
//    TAP_MID     → kbDel()          (delete last char)
//    TAP_RIGHT   → kbOk()           (save + connect)
//
//  STATE_ADV_INFO dispatch:
//    TAP_LEFT or SWIPE_LEFT → goBack()
//
//  STATE_AP_ACTIVE dispatch:
//    TAP_LEFT or SWIPE_LEFT → stopAP() → STATE_STATUS
//
//  All other states:
//    SWIPE_UP    → menuPrev()
//    SWIPE_DOWN  → menuNext()
//    SWIPE_LEFT  → goBack()
//    SWIPE_RIGHT → doSelect()
//    TAP_LEFT    → menuNext()
//    TAP_MID     → menuNext()
//    TAP_RIGHT   → doSelect()
// ============================================================

// ── Gesture tracking state ────────────────────────────────────
static int16_t       _gStartX        = 0;
static int16_t       _gStartY        = 0;
static int16_t       _gEndX          = 0;
static int16_t       _gEndY          = 0;
static bool          _gActive        = false;
static int16_t       _lastSwipeAbsDy = 0;  // vertical magnitude of last swipe (px)

// ── KB live-drag state ────────────────────────────────────────
// Saved when finger touches down in STATE_ESP_KB so that live
// updates are always relative to the same baseline, not compounded.
static int16_t       _kbDragBaseIdx  = 0;   // kbCharIdx at touch-down
static bool          _kbLiveApplied  = false; // true if drag already moved the index

static unsigned long lastTouchMs  = 0;
static bool          touchWasDown = false;


// ============================================================
//  GESTURE CLASSIFIER
//  Returns ACT_NONE if the displacement doesn't qualify as
//  any recognised gesture.
// ============================================================
static TouchAction _classifyGesture() {
  int dx    = (int)_gEndX - (int)_gStartX;
  int dy    = (int)_gEndY - (int)_gStartY;
  int absDx = abs(dx);
  int absDy = abs(dy);

  // ── Swipe check (anywhere on screen, dominant axis) ────────
  if (absDx >= SWIPE_MIN_PX || absDy >= SWIPE_MIN_PX) {
    if (absDx >= absDy) {
      return (dx < 0) ? ACT_SWIPE_LEFT : ACT_SWIPE_RIGHT;
    } else {
      _lastSwipeAbsDy = (int16_t)absDy;   // save magnitude for KB step scaling
      return (dy < 0) ? ACT_SWIPE_UP : ACT_SWIPE_DOWN;
    }
  }

  // ── Tap check (only from physical touch bar, y >= TOUCH_TOP) ─
  if (absDx <= TAP_MAX_PX && absDy <= TAP_MAX_PX &&
      _gStartY >= TOUCH_TOP) {
    int w3 = SCREEN_W / 3;          // 57 px per zone
    if (_gStartX < w3)       return ACT_TAP_LEFT;
    if (_gStartX < w3 * 2)   return ACT_TAP_MID;
    return ACT_TAP_RIGHT;
  }

  return ACT_NONE;
}


// ============================================================
//  TOUCH POLLING  (call from loop() every iteration)
// ============================================================
void handleTouch() {
  TouchPoint tp;
  bool pressed = axs5106l_read(tp);

  if (pressed && !touchWasDown) {
    // ── Finger just touched down ─────────────────────────────
    if (displayFlipped) {              // mirror for 180° rotation
      tp.x = SCREEN_W - 1 - tp.x;
      tp.y = SCREEN_H - 1 - tp.y;
    }
    _gStartX = tp.x;
    _gStartY = tp.y;
    _gEndX   = tp.x;
    _gEndY   = tp.y;
    _gActive = true;

    // Save KB baseline so live drag is always relative to this moment
    if (currentState == STATE_ESP_KB) {
      _kbDragBaseIdx = kbCharIdx;
      _kbLiveApplied = false;
    }

  } else if (pressed && _gActive) {
    // ── Finger still down — track current position ───────────
    if (displayFlipped) {              // mirror for 180° rotation
      tp.x = SCREEN_W - 1 - tp.x;
      tp.y = SCREEN_H - 1 - tp.y;
    }
    _gEndX = tp.x;
    _gEndY = tp.y;

    // ── Live KB scroll (STATE_ESP_KB only, vertical dominant) ─
    if (currentState == STATE_ESP_KB) {
      int dy    = (int)_gEndY - (int)_gStartY;
      int dx    = (int)_gEndX - (int)_gStartX;
      int absDy = abs(dy);
      int absDx = abs(dx);
      if (absDy >= SWIPE_MIN_PX && absDy > absDx) {
        // Vertical dominant — update character index live from baseline
        int steps = _kbSwipeSteps(absDy);
        if (dy < 0) steps = +steps;   // finger up   → next char
        else        steps = -steps;   // finger down → prev char
        int len = (int)strlen(KB_RANGES[kbRangeIdx]);
        if (len > 0) {
          int newIdx = ((int)_kbDragBaseIdx + steps);
          newIdx = ((newIdx % len) + len) % len;
          if (newIdx != kbCharIdx) {
            kbCharIdx      = newIdx;
            _kbLiveApplied = true;
            updateContentArea();
          }
        }
      }
    }

  } else if (!pressed && touchWasDown && _gActive) {
    // ── Finger lifted — classify + dispatch ──────────────────
    // Note: do NOT read tp here; it is stale/zero when not pressed.
    _gActive = false;

    unsigned long now = millis();
    if ((now - lastTouchMs) >= TOUCH_DEBOUNCE_MS) {
      lastTouchMs = now;
      TouchAction act = _classifyGesture();
      if (act != ACT_NONE) {
        dispatchAction(act);
      }
    }
  }

  touchWasDown = pressed;
}


// ============================================================
//  KB HELPERS  (STATE_ESP_KB only)
// ============================================================

// Previous range tab — wraps around
void kbRangePrev() {
  kbRangeIdx = (kbRangeIdx + KB_RANGE_COUNT - 1) % KB_RANGE_COUNT;
  kbCharIdx  = 0;
  updateContentArea();
}

// Next range tab — wraps around
void kbRangeNext() {
  kbRangeIdx = (kbRangeIdx + 1) % KB_RANGE_COUNT;
  kbCharIdx  = 0;
  updateContentArea();
}

// Move highlight up (previous character in current range)
void kbCharPrev() {
  int len = (int)strlen(KB_RANGES[kbRangeIdx]);
  if (len == 0) return;
  kbCharIdx = (kbCharIdx + len - 1) % len;
  updateContentArea();
}

// Move highlight down (next character in current range)
void kbCharNext() {
  int len = (int)strlen(KB_RANGES[kbRangeIdx]);
  if (len == 0) return;
  kbCharIdx = (kbCharIdx + 1) % len;
  updateContentArea();
}

// Map vertical swipe distance (px) to a step count 1–5.
// Screen height is 320 px; SWIPE_MIN_PX ≈ 22.
// Bands are roughly 60 px wide so a near-full-screen swipe gives 5 steps.
//   22–79  px → 1
//   80–139 px → 2
//   140–199 px → 3
//   200–259 px → 4
//   260+    px → 5
static int _kbSwipeSteps(int absDy) {
  if (absDy < 80)  return 1;
  if (absDy < 140) return 2;
  if (absDy < 200) return 3;
  if (absDy < 260) return 4;
  return 5;
}

// Step the KB highlight by `steps` positions (positive = next, negative = prev).
// Wraps around within the current range.
void kbCharStep(int steps) {
  int len = (int)strlen(KB_RANGES[kbRangeIdx]);
  if (len == 0) return;
  // Use modulo-safe arithmetic for both positive and negative steps
  kbCharIdx = ((kbCharIdx + steps) % len + len) % len;
  updateContentArea();
}

// Insert currently highlighted character into password buffer
void kbInsertCurrent() {
  const char *range = KB_RANGES[kbRangeIdx];
  int len = (int)strlen(range);
  if (len == 0) return;
  int idx = kbCharIdx;
  if (idx < 0 || idx >= len) idx = 0;
  char c = range[idx];
  if (kbPassLen < (int)(sizeof(kbPass) - 1)) {
    kbPass[kbPassLen++] = c;
    kbPass[kbPassLen]   = '\0';
  }
  updateContentArea();
}

// Delete last character from password buffer
void kbDel() {
  if (kbPassLen > 0) {
    kbPassLen--;
    kbPass[kbPassLen] = '\0';
  }
  updateContentArea();
}

// Confirm: save SSID+pass to NVS, try to connect, go to STATUS
void kbOk() {
  // Find first empty NVS slot; overwrite last slot if all full
  int freeSlot = -1;
  for (int i = 0; i < MAX_NVS_SLOTS; i++) {
    if (!nvsProfiles[i].valid) { freeSlot = i; break; }
  }
  if (freeSlot < 0) freeSlot = MAX_NVS_SLOTS - 1;

  saveNvsSlot(freeSlot, kbPendingSSID, kbPass,
              true /*dhcp*/, "", "255.255.255.0", "", "");
  loadNvsProfiles();   // refresh in-memory cache

  bool ok = wifiConnectSlot(freeSlot);
  if (ok) espActiveSlot = freeSlot;

  drawStatusBar();
  gfx->flush();

  currentState = STATE_STATUS;
  updateContentArea();
}


// ============================================================
//  NAVIGATION HELPERS
// ============================================================

// Go back to the parent state (or do nothing at root states).
void goBack() {
  switch (currentState) {

    case STATE_ESP_KB:
      // Reset keyboard state and return to scan list
      kbCharIdx  = 0;
      kbRangeIdx = 0;
      kbPass[0]  = '\0';
      kbPassLen  = 0;
      currentState = STATE_ESP_SCAN;
      break;

    case STATE_ESP_SCAN:
      currentState = STATE_ESP_MENU;
      break;

    case STATE_ESP_MENU:
    case STATE_NVS_MENU:
    case STATE_PI_MENU:
    case STATE_ADVANCED:
      currentState = STATE_MENU;
      break;

    case STATE_ADV_INFO:
      currentState = STATE_ADVANCED;
      break;

    case STATE_AP_SELECT:
      advMenuIndex = 0;
      currentState = STATE_ADVANCED;
      break;

    case STATE_MENU:
      currentState = STATE_STATUS;
      break;

    // STATE_STATUS, STATE_AP_ACTIVE: nothing to go back to
    default:
      return;
  }
  updateContentArea();
}

// Scroll to next item in the current list (wraps around).
void menuNext() {
  switch (currentState) {

    case STATE_STATUS:
      // Entering the menu for the first time
      menuIndex    = 0;
      currentState = STATE_MENU;
      updateContentArea();
      break;

    case STATE_MENU:
      menuIndex = (menuIndex + 1) % 4;
      updateContentArea();
      break;

    case STATE_PI_MENU: {
      int total = numDynProfiles + 1;   // profiles + Back
      if (total < 1) total = 1;
      piMenuIndex = (piMenuIndex + 1) % total;
      updateContentArea();
      break;
    }

    case STATE_NVS_MENU:
      nvsMenuIndex = (nvsMenuIndex + 1) % (MAX_NVS_SLOTS + 1);
      updateContentArea();
      break;

    case STATE_ESP_MENU:
      // items: 10 slots + "ESP Scan..." + "Pi Scan..." + "<- Back"
      espMenuIndex = (espMenuIndex + 1) % (MAX_NVS_SLOTS + 3);
      updateContentArea();
      break;

    case STATE_ESP_SCAN:
      if (scanCount > 0)
        scanIndex = (scanIndex + 1) % (scanCount + 1);
      updateContentArea();
      break;

    case STATE_PI_SCAN:
      if (piScanCount > 0)
        piScanIndex = (piScanIndex + 1) % (piScanCount + 1);
      updateContentArea();
      break;

    case STATE_ADVANCED:
      advMenuIndex = (advMenuIndex + 1) % 5;
      updateContentArea();
      break;

    case STATE_AP_SELECT:
      apSelectIndex = (apSelectIndex + 1) % (MAX_AP_PROFILES + 1);
      updateContentArea();
      break;

    default:
      break;
  }
}

// Scroll to previous item in the current list (wraps around).
void menuPrev() {
  switch (currentState) {

    case STATE_MENU:
      menuIndex = (menuIndex + 3) % 4;
      updateContentArea();
      break;

    case STATE_PI_MENU: {
      int total = numDynProfiles + 1;
      if (total < 1) total = 1;
      piMenuIndex = (piMenuIndex + total - 1) % total;
      updateContentArea();
      break;
    }

    case STATE_NVS_MENU: {
      int total = MAX_NVS_SLOTS + 1;
      nvsMenuIndex = (nvsMenuIndex + total - 1) % total;
      updateContentArea();
      break;
    }

    case STATE_ESP_MENU: {
      int total = MAX_NVS_SLOTS + 3;
      espMenuIndex = (espMenuIndex + total - 1) % total;
      updateContentArea();
      break;
    }

    case STATE_ESP_SCAN:
      if (scanCount > 0) {
        int total = scanCount + 1;
        scanIndex = (scanIndex + total - 1) % total;
        updateContentArea();
      }
      break;

    case STATE_PI_SCAN:
      if (piScanCount > 0) {
        int total = piScanCount + 1;
        piScanIndex = (piScanIndex + total - 1) % total;
        updateContentArea();
      }
      break;

    case STATE_ADVANCED:
      advMenuIndex = (advMenuIndex + 4) % 5;
      updateContentArea();
      break;

    case STATE_AP_SELECT:
      apSelectIndex = (apSelectIndex + MAX_AP_PROFILES) % (MAX_AP_PROFILES + 1);
      updateContentArea();
      break;

    default:
      break;
  }
}

// Activate / open the currently selected item.
void doSelect() {
  switch (currentState) {

    // ── Home → main menu ──────────────────────────────────────
    case STATE_STATUS:
      menuIndex    = 0;
      currentState = STATE_MENU;
      updateContentArea();
      break;

    // ── Top-level menu ────────────────────────────────────────
    case STATE_MENU:
      if (menuIndex == 0) {
        piMenuIndex  = 0;
        currentState = STATE_PI_MENU;
      } else if (menuIndex == 1) {
        espMenuIndex = 0;
        currentState = STATE_ESP_MENU;
      } else if (menuIndex == 2) {
        nvsMenuIndex = 0;
        currentState = STATE_NVS_MENU;
      } else {
        advMenuIndex = 0;
        currentState = STATE_ADVANCED;
      }
      updateContentArea();
      break;

    // ── Pi network profiles ───────────────────────────────────
    case STATE_PI_MENU:
      if (numDynProfiles == 0 || piMenuIndex == numDynProfiles) {
        goBack();
      } else {
        Serial.print("CMD:CONNECT:");
        Serial.println(dynProfiles[piMenuIndex]);
        currentState = STATE_STATUS;
        updateContentArea();
      }
      break;

    // ── NVS slots → push to Pi ────────────────────────────────
    case STATE_NVS_MENU:
      if (nvsMenuIndex == MAX_NVS_SLOTS) {
        goBack();
      } else if (nvsProfiles[nvsMenuIndex].valid) {
        NvsProfile &p = nvsProfiles[nvsMenuIndex];
        // Pending feedback until OK/ERR:ADDWIFI_FULL arrives ("\x84" = 'ä' in the CP437 GFX font)
        hasError = false;
        hasInfo  = true;
        infoExpireMs = millis() + 30000UL;   // nmcli can be slow; give up after 30 s
        strlcpy(infoMsg, "L\x84gger till WiFi...", sizeof(infoMsg));
        Serial.print("CMD:ADDWIFI_FULL:");
        Serial.print(p.ssid);            Serial.print("|");
        Serial.print(p.pass);            Serial.print("|");
        Serial.print(p.dhcp ? "1" : "0"); Serial.print("|");
        Serial.print(p.ip);              Serial.print("|");
        Serial.print(p.sn);              Serial.print("|");
        Serial.print(p.gw);              Serial.print("|");
        Serial.println(p.dns);
        currentState = STATE_STATUS;
        updateContentArea();
      }
      // Empty slot → stay in menu
      break;

    // ── ESP Wi-Fi menu ────────────────────────────────────────
    case STATE_ESP_MENU: {
      int espScanIdx = MAX_NVS_SLOTS;
      int piScanIdx  = MAX_NVS_SLOTS + 1;
      int backIdx    = MAX_NVS_SLOTS + 2;

      if (espMenuIndex == backIdx) {
        goBack();
      } else if (espMenuIndex == piScanIdx) {
        // Start Pi-side Wi-Fi scan (nmcli, 2.4 + 5 GHz)
        piScanBusy  = true;
        piScanCount = 0;
        piScanIndex = 0;
        currentState = STATE_PI_SCAN;
        updateContentArea();
        Serial.println("CMD:SCAN_WIFI");
      } else if (espMenuIndex == espScanIdx) {
        // Start async scan and move to scan screen
        scanBusy  = true;
        scanCount = 0;
        scanIndex = 0;
        currentState = STATE_ESP_SCAN;
        updateContentArea();
        wifiScanStart();
      } else if (nvsProfiles[espMenuIndex].valid) {
        // Connect ESP to saved profile
        bool ok = wifiConnectSlot(espMenuIndex);
        if (ok) espActiveSlot = espMenuIndex;
        drawStatusBar();
        gfx->flush();
        currentState = STATE_STATUS;
        updateContentArea();
      }
      // Empty slot → stay
      break;
    }

    // ── ESP scan results → open keyboard ─────────────────────
    case STATE_ESP_SCAN:
      if (scanIndex == scanCount) {
        goBack();
      } else {
        strlcpy(kbPendingSSID, scanSSIDs[scanIndex], sizeof(kbPendingSSID));
        kbCharIdx  = 0;
        kbRangeIdx = 0;
        kbPass[0]  = '\0';
        kbPassLen  = 0;
        currentState = STATE_ESP_KB;
        updateContentArea();
      }
      break;

    // ── Pi scan results → open keyboard ───────────────────────
    case STATE_PI_SCAN:
      if (piScanIndex == piScanCount) {
        goBack();
      } else {
        strlcpy(kbPendingSSID, piScanSSIDs[piScanIndex], sizeof(kbPendingSSID));
        kbCharIdx  = 0;
        kbRangeIdx = 0;
        kbPass[0]  = '\0';
        kbPassLen  = 0;
        currentState = STATE_ESP_KB;
        updateContentArea();
      }
      break;

    // ── Advanced sub-menu ─────────────────────────────────────
    case STATE_ADVANCED:
      if (advMenuIndex == 0) {
        // Pick an AP profile first (STATE_AP_SELECT)
        apSelectIndex = activeApProfile;
        currentState  = STATE_AP_SELECT;
        updateContentArea();
      } else if (advMenuIndex == 1) {
        currentState = STATE_ADV_INFO;
        updateContentArea();
      } else if (advMenuIndex == 2) {
        // Install rig_daemon.py — wipes content area then runs
        gfx->fillRect(0, CONTENT_TOP, SCREEN_W, CONTENT_H, COL_BG);
        gfx->setTextSize(1);
        gfx->flush();
        hidInstallOnPi();
        // hidInstallOnPi() leaves its status on the display.
        // Stay in STATE_ADVANCED; a redraw is NOT forced here
        // so the user can read the result before moving on.
      } else if (advMenuIndex == 3) {
        // ── Flip display 180° ──────────────────────────────────
        displayFlipped = !displayFlipped;
        gfx->setRotation(displayFlipped ? 2 : 0);
        { Preferences _p; _p.begin("rigcfg", false);
          _p.putBool("flip", displayFlipped); _p.end(); }
        gfx->fillScreen(COL_BG);
        updateContentArea();
      } else {
        goBack();
      }
      break;

    // ── AP profile quick-pick ─────────────────────────────────
    case STATE_AP_SELECT:
      if (apSelectIndex < MAX_AP_PROFILES) {
        startAP(apSelectIndex);
        currentState = STATE_AP_ACTIVE;
      } else {
        advMenuIndex = 0;
        currentState = STATE_ADVANCED;   // "<- Back"
      }
      updateContentArea();
      break;

    // ── Terminal states — SELECT does nothing ─────────────────
    case STATE_ADV_INFO:
    case STATE_AP_ACTIVE:
    case STATE_ESP_KB:     // handled entirely by gesture mapping
    default:
      break;
  }
}


// ============================================================
//  ACTION DISPATCHER
// ============================================================
void dispatchAction(TouchAction act) {

  // ── Keyboard: custom gesture mapping ─────────────────────────
  if (currentState == STATE_ESP_KB) {
    switch (act) {
      case ACT_SWIPE_LEFT:  kbRangePrev();  break;   // always 1 range step
      case ACT_SWIPE_RIGHT: kbRangeNext();  break;   // always 1 range step
      case ACT_SWIPE_UP:                             // finger up → next char(s)
        if (!_kbLiveApplied)                         // skip if live drag already set position
          kbCharStep(+_kbSwipeSteps(_lastSwipeAbsDy));
        _kbLiveApplied = false;
        break;
      case ACT_SWIPE_DOWN:                           // finger down → prev char(s)
        if (!_kbLiveApplied)
          kbCharStep(-_kbSwipeSteps(_lastSwipeAbsDy));
        _kbLiveApplied = false;
        break;
      case ACT_TAP_LEFT:    kbInsertCurrent();  break;
      case ACT_TAP_MID:     kbDel();            break;
      case ACT_TAP_RIGHT:   kbOk();             break;
      default: break;
    }
    return;
  }

  // ── AP Active: only left tap / swipe left closes AP ──────────
  if (currentState == STATE_AP_ACTIVE) {
    if (act == ACT_TAP_LEFT || act == ACT_SWIPE_LEFT) {
      stopAP();
      currentState = STATE_STATUS;
      updateContentArea();
    }
    return;
  }

  // ── Adv Info: only left tap / swipe left goes back ───────────
  if (currentState == STATE_ADV_INFO) {
    if (act == ACT_TAP_LEFT || act == ACT_SWIPE_LEFT) {
      goBack();
    }
    return;
  }

  // ── All other states: generic navigation ─────────────────────
  switch (act) {
    case ACT_SWIPE_UP:    menuPrev();  break;
    case ACT_SWIPE_DOWN:  menuNext();  break;
    case ACT_SWIPE_LEFT:  goBack();   break;
    case ACT_SWIPE_RIGHT: doSelect(); break;
    case ACT_TAP_LEFT:    menuNext(); break;
    case ACT_TAP_MID:     menuNext(); break;
    case ACT_TAP_RIGHT:   doSelect(); break;
    default: break;
  }
}


// ============================================================
//  SCAN RESULT POLLING
//  Called from loop() while currentState == STATE_ESP_SCAN.
// ============================================================
void pollWifiScan() {
  if (!scanBusy) return;
  int n = wifiScanResults(scanSSIDs, MAX_SCAN_RESULTS);
  if (n < 0) return;   // still running
  scanCount = n;
  scanBusy  = false;
  updateContentArea();
}

// ============================================================
//  PI SCAN RESULT POLLING
//  Pi scan data arrives via SCAN_RESULT: / SCAN_DONE messages
//  handled in protocol.ino — no polling needed here.
//  Called from loop(); currently a no-op placeholder.
// ============================================================
void pollPiScan() {
  // Data is injected directly into piScanSSIDs[] by protocol.ino.
  // SCAN_DONE calls updateContentArea() when the scan finishes.
  (void)piScanBusy;  // suppress unused-variable warning
}


// ============================================================
//  CENTRAL SCREEN UPDATER
//  Clears the content area, draws the appropriate content +
//  header + touch bar + status bar, then flushes the canvas.
// ============================================================
void updateContentArea() {
  gfx->fillRect(0, CONTENT_TOP, SCREEN_W, CONTENT_H, COL_BG);

  switch (currentState) {

    case STATE_STATUS:
      drawHeader("HMS Rig Console");
      drawHomeContent();
      drawTouchBar("MENU", "SELECT");
      break;

    case STATE_MENU:
      drawHeader("Main Menu");
      drawTopMenuContent();
      drawTouchBar("NEXT", "OPEN");
      break;

    case STATE_PI_MENU:
      drawHeader("Pi Networks");
      drawPiMenuContent();
      drawTouchBar("NEXT", "SELECT");
      break;

    case STATE_NVS_MENU:
      drawHeader("-> Pi Profile");
      drawNvsMenuContent();
      drawTouchBar("NEXT", "SEND");
      break;

    case STATE_ESP_MENU:
      drawHeader("ESP Wi-Fi");
      drawEspMenuContent();
      drawTouchBar("NEXT", "SELECT");
      break;

    case STATE_ESP_SCAN:
      drawHeader("ESP Scan Results");
      drawEspScanContent();
      drawTouchBar("NEXT", "SELECT");
      break;

    case STATE_PI_SCAN:
      drawHeader("Pi Scan Results");
      drawPiScanContent();
      drawTouchBar("NEXT", "SELECT");
      break;

    case STATE_ESP_KB:
      drawHeader("Enter Password");
      drawEspKbContent();
      drawTouchBarKb();     // 3-zone: NEXT | DEL | OK
      break;

    case STATE_ADVANCED:
      drawHeader("Advanced");
      drawAdvancedMenuContent();
      drawTouchBar("NEXT", "OPEN");
      break;

    case STATE_ADV_INFO:
      drawHeader("System Info");
      drawAdvInfoContent();
      drawTouchBar("BACK", "---");
      break;

    case STATE_AP_SELECT:
      drawHeader("Start AP");
      drawApSelectContent();
      drawTouchBar("NEXT", "START");
      break;

    case STATE_AP_ACTIVE:
      drawHeader("AP Active");
      drawApActiveContent();
      drawTouchBar("STOP AP", "---");
      break;
  }

  drawStatusBar();
  gfx->flush();
}
