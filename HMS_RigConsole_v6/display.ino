// ============================================================
//  display.ino — All draw functions for HMS Rig Console v4
//
//  Screen layout (portrait 172×320):
//    y=  0.. 19   drawHeader()       — 20 px
//    y= 20..249   content area       — 230 px  (MENU_ROW_H=18)
//    y=250..299   drawStatusBar()    — 50 px   (3 lines)
//    y=300..319   drawTouchBar()     — 20 px
//
//  Status bar lines:
//    SB_LINE1 (y=254)  ESP Wi-Fi SSID + IP
//    SB_LINE2 (y=268)  Pi online/offline + Pi active SSID
//    SB_LINE3 (y=282)  USB port / error detail
// ============================================================

// ── Text helpers ─────────────────────────────────────────────

static void drawTxt(const char *s, int16_t x, int16_t y,
                    uint16_t fg, uint16_t bg) {
  gfx->setTextColor(fg, bg);
  gfx->setCursor(x, y);
  gfx->print(s);
}

static void drawCentredTxt(const char *s, int16_t cx, int16_t y,
                            uint16_t fg, uint16_t bg) {
  int16_t x1, y1; uint16_t tw, th;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &tw, &th);
  gfx->setTextColor(fg, bg);
  gfx->setCursor(cx - (int16_t)(tw / 2), y);
  gfx->print(s);
}

// Truncate string to maxChars into buf (including NUL)
static void truncate(char *buf, const char *src, int maxChars) {
  strlcpy(buf, src, maxChars + 1);
}


// ============================================================
//  HEADER BAR
// ============================================================
void drawHeader(const char *title) {
  gfx->fillRect(0, 0, SCREEN_W, HDR_H, COL_HDR_BG);
  gfx->drawFastHLine(0, HDR_H - 1, SCREEN_W, COL_ACCENT);
  gfx->setTextSize(1);
  drawTxt(title, 14, 6, COL_HDR_TXT, COL_HDR_BG);
  int cx = SCREEN_W - HMS_LOGO_CORNER_W - 12;
  int cy = (HDR_H - HMS_LOGO_CORNER_H) / 2;
  drawProgmemImage(cx, cy, HMS_LOGO_CORNER_W, HMS_LOGO_CORNER_H, hms_logo_corner);
}


// ============================================================
//  STATUS BAR  — 3 lines, 50 px total
// ============================================================
void drawStatusBar() {
  gfx->fillRect(0, STATUS_TOP, SCREEN_W, STATUS_H, COL_STATUS_BG);
  gfx->drawFastHLine(0, STATUS_TOP, SCREEN_W, COL_ACCENT);
  gfx->setTextSize(1);

  int y1 = STATUS_TOP + SB_LINE1;
  int y2 = STATUS_TOP + SB_LINE2;
  int y3 = STATUS_TOP + SB_LINE3;

  // ── Line 1: ESP Wi-Fi ────────────────────────────────────
  if (espConnected) {
    char wfBuf[28];
    snprintf(wfBuf, sizeof(wfBuf), "%.12s  %.15s", espSSID, espIP);
    drawTxt(wfBuf, 4, y1, COL_ACTIVE, COL_STATUS_BG);
  } else {
    drawTxt("ESP: no Wi-Fi", 4, y1, COL_DIM, COL_STATUS_BG);
  }

  // ── Line 2: Pi status ────────────────────────────────────
  if (piConnected) {
    drawTxt("Pi: ", 4, y2, COL_DIM, COL_STATUS_BG);
    char ssBuf[22];
    truncate(ssBuf, activeSSID, 21);
    drawTxt(ssBuf, 28, y2, COL_ACTIVE, COL_STATUS_BG);
  } else {
    drawTxt("Pi: offline", 4, y2, COL_ERR, COL_STATUS_BG);
  }

  // ── Line 3: Pi IP or error ───────────────────────────────
  if (hasError) {
    char errBuf[24];
    truncate(errBuf, lastErrMsg, 23);
    drawTxt(errBuf, 4, y3, COL_ERR, COL_STATUS_BG);
  } else if (hasInfo) {
    char infoBuf[24];
    truncate(infoBuf, infoMsg, 23);
    drawTxt(infoBuf, 4, y3, COL_ACCENT, COL_STATUS_BG);
  } else if (piConnected) {
    char ipBuf[24];
    truncate(ipBuf, activeIP, 23);
    drawTxt(ipBuf, 4, y3, COL_DIM, COL_STATUS_BG);
  } else {
    drawTxt("Connect Pi via USB", 4, y3, COL_DIM, COL_STATUS_BG);
  }
}


// ============================================================
//  TOUCH BAR — 2 zones  (standard menus)
// ============================================================
void drawTouchBar(const char *leftLabel, const char *rightLabel) {
  gfx->fillRect(0, TOUCH_TOP, SCREEN_W, TOUCH_H, COL_STATUS_BG);
  gfx->drawFastHLine(0, TOUCH_TOP, SCREEN_W, COL_BORDER);

  int mid  = SCREEN_W / 2;
  int midY = TOUCH_TOP + TOUCH_H / 2 - 4;

  gfx->fillRoundRect(2, TOUCH_TOP + 2, mid - 4, TOUCH_H - 4, 3, COL_HEADER);
  gfx->drawRoundRect(2, TOUCH_TOP + 2, mid - 4, TOUCH_H - 4, 3, COL_BORDER);
  gfx->setTextSize(1);
  drawCentredTxt(leftLabel,  mid / 2,       midY, COL_TEXT,  COL_HEADER);

  gfx->fillRoundRect(mid + 2, TOUCH_TOP + 2, mid - 4, TOUCH_H - 4, 3, COL_ACCENT);
  drawCentredTxt(rightLabel, mid + mid / 2, midY, 0x0000, COL_ACCENT);
}


// ============================================================
//  TOUCH BAR — 3 zones  (keyboard: NEXT | DEL | OK)
// ============================================================
void drawTouchBarKb() {
  gfx->fillRect(0, TOUCH_TOP, SCREEN_W, TOUCH_H, COL_STATUS_BG);
  gfx->drawFastHLine(0, TOUCH_TOP, SCREEN_W, COL_BORDER);

  int w    = SCREEN_W / 3;          // 57 px each
  int midY = TOUCH_TOP + TOUCH_H / 2 - 4;

  // Left: NEXT (insert current char)
  gfx->fillRoundRect(2, TOUCH_TOP + 2, w - 4, TOUCH_H - 4, 3, COL_HEADER);
  gfx->drawRoundRect(2, TOUCH_TOP + 2, w - 4, TOUCH_H - 4, 3, COL_BORDER);
  gfx->setTextSize(1);
  drawCentredTxt("NEXT", w / 2, midY, COL_TEXT, COL_HEADER);

  // Mid: DEL
  gfx->fillRoundRect(w + 2, TOUCH_TOP + 2, w - 4, TOUCH_H - 4, 3, COL_HEADER);
  gfx->drawRoundRect(w + 2, TOUCH_TOP + 2, w - 4, TOUCH_H - 4, 3, COL_BORDER);
  drawCentredTxt("DEL", w + w / 2, midY, COL_ERR, COL_HEADER);

  // Right: OK
  int rightW = SCREEN_W - w * 2;
  gfx->fillRoundRect(w * 2 + 2, TOUCH_TOP + 2, rightW - 4, TOUCH_H - 4, 3, COL_ACCENT);
  drawCentredTxt("OK", w * 2 + rightW / 2, midY, 0x0000, COL_ACCENT);
}


// ============================================================
//  GENERIC COMPACT LIST RENDERER
//  Used by all menus.  Each item is MENU_ROW_H pixels tall.
//  Highlighted item has COL_HEADER background + accent border.
// ============================================================
static void drawMenuList(const MenuItem *items, int count,
                          int selected, int windowStart) {
  gfx->setTextSize(1);
  int maxVisible = CONTENT_H / MENU_ROW_H;

  for (int slot = 0; slot < maxVisible; slot++) {
    int i = windowStart + slot;
    if (i >= count) break;
    bool     isHl = (i == selected);
    int      y    = CONTENT_TOP + slot * MENU_ROW_H;
    uint16_t bg   = isHl ? COL_HEADER : COL_BG;
    uint16_t fg   = isHl ? COL_ACCENT  : items[i].fg;

    gfx->fillRect(0, y, SCREEN_W, MENU_ROW_H - 1, bg);
    if (isHl) gfx->drawRect(0, y, SCREEN_W, MENU_ROW_H - 1, COL_ACCENT);

    int textY = y + (MENU_ROW_H - 8) / 2;   // vertically centre 8 px text
    drawTxt(">", 4, textY, isHl ? COL_ACCENT : COL_BG, bg);
    drawTxt(items[i].label, 14, textY, fg, bg);
  }
}

// Compute window start for scrolling lists
static int menuWindowStart(int selected, int count, int maxVisible) {
  int ws = 0;
  if (selected >= maxVisible) ws = selected - maxVisible + 1;
  if (ws + maxVisible > count) ws = count - maxVisible;
  if (ws < 0) ws = 0;
  return ws;
}


// ============================================================
//  HOME SCREEN
// ============================================================
void drawHomeContent() {
  gfx->setTextSize(1);
  int y = CONTENT_TOP + 6;
  int R = 16;

  drawTxt("Network Manipulator", 4, y, COL_DIM, COL_BG);  y += R;
  drawTxt("Front Panel Control", 4, y, COL_DIM, COL_BG);  y += R;
  gfx->drawFastHLine(4, y, SCREEN_W - 8, COL_BORDER);     y += 6;

  drawTxt("Pi link:", 4,  y, COL_DIM, COL_BG);
  drawTxt(piConnected ? "OK" : "Waiting...",
          60, y, piConnected ? COL_ACTIVE : COL_DIM, COL_BG);
  y += R;

  drawTxt("Serial:", 4, y, COL_DIM, COL_BG);
  drawTxt("115200", 60, y, COL_ACTIVE, COL_BG);
  y += R;

  gfx->drawFastHLine(4, y, SCREEN_W - 8, COL_BORDER);     y += 6;

  if (piConnected)
    drawTxt("MENU: select network", 4, y, COL_DIM, COL_BG);
  else
    drawTxt("Connect Pi via USB",   4, y, COL_DIM, COL_BG);
}


// ============================================================
//  TOP-LEVEL MENU  (4 items)
// ============================================================
void drawTopMenuContent() {
  static const MenuItem items[] = {
    { "Pi networks",    COL_TEXT },
    { "ESP Wi-Fi",      COL_TEXT },
    { "Local profiles", COL_TEXT },
    { "Advanced",       COL_TEXT },
  };
  drawMenuList(items, 4, menuIndex, 0);
}


// ============================================================
//  PI PROFILES  (nmcli profiles from Pi)
// ============================================================
void drawPiMenuContent() {
  if (numDynProfiles == 0) {
    gfx->setTextSize(1);
    drawCentredTxt("Pi not connected.", SCREEN_W / 2,
                   CONTENT_TOP + 80, COL_DIM, COL_BG);
    drawCentredTxt("Connect USB cable.", SCREEN_W / 2,
                   CONTENT_TOP + 96, COL_DIM, COL_BG);
    return;
  }

  int total      = numDynProfiles + 1;   // + Back
  int maxVisible = CONTENT_H / MENU_ROW_H;
  int ws         = menuWindowStart(piMenuIndex, total, maxVisible);

  gfx->setTextSize(1);
  for (int slot = 0; slot < maxVisible; slot++) {
    int  i      = ws + slot;
    if (i >= total) break;
    bool isHl   = (i == piMenuIndex);
    bool isBack = (i == numDynProfiles);
    // Compare against activeConn (NM connection name), not activeSSID (SSID).
    // dynProfiles[] holds NM connection names from the Pi.
    bool isAt   = (!isBack && strcmp(dynProfiles[i], activeConn) == 0);
    int      y  = CONTENT_TOP + slot * MENU_ROW_H;
    uint16_t bg = isHl ? COL_HEADER : COL_BG;
    uint16_t fg = isBack ? (isHl ? COL_ACCENT : COL_DIM)
                : isAt   ? COL_ACTIVE : COL_TEXT;

    gfx->fillRect(0, y, SCREEN_W, MENU_ROW_H - 1, bg);
    if (isHl) gfx->drawRect(0, y, SCREEN_W, MENU_ROW_H - 1, COL_ACCENT);

    int ty = y + (MENU_ROW_H - 8) / 2;
    drawTxt(">", 4, ty, isHl ? COL_ACCENT : COL_BG, bg);
    const char *label = isBack ? "<- Back" : dynProfiles[i];
    drawTxt(label, 14, ty, fg, bg);
    if (isAt) gfx->fillCircle(SCREEN_W - 7, y + MENU_ROW_H / 2, 3, COL_ACTIVE);

    // Scroll indicators
    if (total > maxVisible) {
      if (ws > 0 && slot == 0)
        drawTxt("^", SCREEN_W - 10, y + 2, COL_DIM, bg);
      if (ws + maxVisible < total && slot == maxVisible - 1)
        drawTxt("v", SCREEN_W - 10, ty, COL_DIM, bg);
    }
  }
}


// ============================================================
//  NVS SLOTS  (shared profiles — push to Pi)
// ============================================================
void drawNvsMenuContent() {
  int total      = MAX_NVS_SLOTS + 1;   // + Back
  int maxVisible = CONTENT_H / MENU_ROW_H;
  int ws         = menuWindowStart(nvsMenuIndex, total, maxVisible);

  gfx->setTextSize(1);
  for (int slot = 0; slot < maxVisible; slot++) {
    int  i      = ws + slot;
    if (i >= total) break;
    bool isHl   = (i == nvsMenuIndex);
    bool isBack = (i == MAX_NVS_SLOTS);
    int      y  = CONTENT_TOP + slot * MENU_ROW_H;
    uint16_t bg = isHl ? COL_HEADER : COL_BG;

    gfx->fillRect(0, y, SCREEN_W, MENU_ROW_H - 1, bg);
    if (isHl) gfx->drawRect(0, y, SCREEN_W, MENU_ROW_H - 1, COL_ACCENT);
    int ty = y + (MENU_ROW_H - 8) / 2;

    if (isBack) {
      drawTxt("<- Back", 14, ty, isHl ? COL_ACCENT : COL_DIM, bg);
    } else {
      char slotLbl[4];
      snprintf(slotLbl, sizeof(slotLbl), "%2d.", i + 1);
      drawTxt(slotLbl, 4, ty, COL_DIM, bg);

      if (nvsProfiles[i].valid) {
        bool isAt = (strcmp(nvsProfiles[i].ssid, espSSID) == 0);
        drawTxt(nvsProfiles[i].ssid, 28, ty,
                isAt ? COL_ACTIVE : COL_TEXT, bg);
        if (isAt) gfx->fillCircle(SCREEN_W - 7, y + MENU_ROW_H / 2, 3, COL_ACTIVE);
      } else {
        drawTxt("-- empty --", 28, ty, COL_DIM, bg);
      }
    }
  }
}


// ============================================================
//  ESP WI-FI MENU  (shared profiles — connect ESP)
//  Items: 10 NVS slots + "ESP Scan..." + "Pi Scan..." + "<- Back"
// ============================================================
void drawEspMenuContent() {
  int total      = MAX_NVS_SLOTS + 3;
  int maxVisible = CONTENT_H / MENU_ROW_H;
  int ws         = menuWindowStart(espMenuIndex, total, maxVisible);

  gfx->setTextSize(1);
  for (int slot = 0; slot < maxVisible; slot++) {
    int  i      = ws + slot;
    if (i >= total) break;
    bool isHl      = (i == espMenuIndex);
    bool isEspScan = (i == MAX_NVS_SLOTS);
    bool isPiScan  = (i == MAX_NVS_SLOTS + 1);
    bool isBack    = (i == MAX_NVS_SLOTS + 2);
    int      y     = CONTENT_TOP + slot * MENU_ROW_H;
    uint16_t bg    = isHl ? COL_HEADER : COL_BG;

    gfx->fillRect(0, y, SCREEN_W, MENU_ROW_H - 1, bg);
    if (isHl) gfx->drawRect(0, y, SCREEN_W, MENU_ROW_H - 1, COL_ACCENT);
    int ty = y + (MENU_ROW_H - 8) / 2;

    if (isBack) {
      drawTxt("<- Back", 14, ty, isHl ? COL_ACCENT : COL_DIM, bg);
    } else if (isPiScan) {
      drawTxt("+ Pi Scan...", 14, ty, isHl ? COL_ACCENT : COL_ACTIVE, bg);
    } else if (isEspScan) {
      drawTxt("+ ESP Scan...", 14, ty, isHl ? COL_ACCENT : COL_ACTIVE, bg);
    } else {
      char slotLbl[4];
      snprintf(slotLbl, sizeof(slotLbl), "%2d.", i + 1);
      drawTxt(slotLbl, 4, ty, COL_DIM, bg);
      if (nvsProfiles[i].valid) {
        bool isAt = (i == espActiveSlot);
        drawTxt(nvsProfiles[i].ssid, 28, ty,
                isAt ? COL_ACTIVE : COL_TEXT, bg);
        if (isAt) gfx->fillCircle(SCREEN_W - 7, y + MENU_ROW_H / 2, 3, COL_ACTIVE);
      } else {
        drawTxt("-- empty --", 28, ty, COL_DIM, bg);
      }
    }
  }
}


// ============================================================
//  ESP WI-FI SCAN RESULTS  (2.4 GHz — ESP32 radio)
//  scanSSIDs / scanCount filled by pollWifiScan() in touch.ino
// ============================================================
void drawEspScanContent() {
  gfx->setTextSize(1);

  if (scanBusy) {
    drawCentredTxt("Scanning...", SCREEN_W / 2,
                   CONTENT_TOP + CONTENT_H / 2 - 8, COL_DIM, COL_BG);
    return;
  }

  if (scanCount == 0) {
    drawCentredTxt("No networks found.", SCREEN_W / 2,
                   CONTENT_TOP + CONTENT_H / 2 - 8, COL_DIM, COL_BG);
    drawCentredTxt("Swipe L: back", SCREEN_W / 2,
                   CONTENT_TOP + CONTENT_H / 2 + 8, COL_DIM, COL_BG);
    return;
  }

  // Items: scan results + "<- Back"
  int total      = scanCount + 1;
  int maxVisible = CONTENT_H / MENU_ROW_H;
  int ws         = menuWindowStart(scanIndex, total, maxVisible);

  for (int slot = 0; slot < maxVisible; slot++) {
    int  i      = ws + slot;
    if (i >= total) break;
    bool isHl   = (i == scanIndex);
    bool isBack = (i == scanCount);
    int      y  = CONTENT_TOP + slot * MENU_ROW_H;
    uint16_t bg = isHl ? COL_HEADER : COL_BG;

    gfx->fillRect(0, y, SCREEN_W, MENU_ROW_H - 1, bg);
    if (isHl) gfx->drawRect(0, y, SCREEN_W, MENU_ROW_H - 1, COL_ACCENT);

    int ty = y + (MENU_ROW_H - 8) / 2;
    if (isBack) {
      drawTxt("<- Back", 14, ty, isHl ? COL_ACCENT : COL_DIM, bg);
    } else {
      drawTxt(scanSSIDs[i], 14, ty, isHl ? COL_ACCENT : COL_TEXT, bg);
    }
  }
}


// ============================================================
//  PI WI-FI SCAN RESULTS  (2.4 + 5 GHz — Pi nmcli)
//  piScanSSIDs / piScanCount filled by SCAN_RESULT: messages in protocol.ino
// ============================================================
void drawPiScanContent() {
  gfx->setTextSize(1);

  if (piScanBusy) {
    drawCentredTxt("Pi scanning...", SCREEN_W / 2,
                   CONTENT_TOP + CONTENT_H / 2 - 8, COL_DIM, COL_BG);
    drawCentredTxt("(5 GHz incl.)", SCREEN_W / 2,
                   CONTENT_TOP + CONTENT_H / 2 + 8, COL_DIM, COL_BG);
    return;
  }

  if (piScanCount == 0) {
    drawCentredTxt("No networks found.", SCREEN_W / 2,
                   CONTENT_TOP + CONTENT_H / 2 - 8, COL_DIM, COL_BG);
    drawCentredTxt("Swipe L: back", SCREEN_W / 2,
                   CONTENT_TOP + CONTENT_H / 2 + 8, COL_DIM, COL_BG);
    return;
  }

  // Items: scan results + "<- Back"
  int total      = piScanCount + 1;
  int maxVisible = CONTENT_H / MENU_ROW_H;
  int ws         = menuWindowStart(piScanIndex, total, maxVisible);

  for (int slot = 0; slot < maxVisible; slot++) {
    int  i      = ws + slot;
    if (i >= total) break;
    bool isHl   = (i == piScanIndex);
    bool isBack = (i == piScanCount);
    int      y  = CONTENT_TOP + slot * MENU_ROW_H;
    uint16_t bg = isHl ? COL_HEADER : COL_BG;

    gfx->fillRect(0, y, SCREEN_W, MENU_ROW_H - 1, bg);
    if (isHl) gfx->drawRect(0, y, SCREEN_W, MENU_ROW_H - 1, COL_ACCENT);

    int ty = y + (MENU_ROW_H - 8) / 2;
    if (isBack) {
      drawTxt("<- Back", 14, ty, isHl ? COL_ACCENT : COL_DIM, bg);
    } else {
      drawTxt(piScanSSIDs[i], 14, ty, isHl ? COL_ACCENT : COL_TEXT, bg);
    }
  }
}


// ============================================================
//  CHARACTER-PICKER KEYBOARD  (gesture + 3-button layout)
//
//  Visual layout ("wheel" style):
//
//    y=22   SSID:...
//    y=34   password field + cursor
//    y=44   ── separator ──
//    y=46   [az] [AZ] [09] [S1] [S2] [In]  ← range tabs
//    y=58   ── separator ──
//    y=62   hint line  "< swipe > range  ^ char v"
//    y=74   ── separator ──
//    y=78   [ prev-2 char ]   (dimmed)
//    y=104  [ prev-1 char ]   (dimmed)
//    y=130  [ CURRENT CHAR ]  (large, highlighted)   ← centre
//    y=160  [ next+1 char ]   (dimmed)
//    y=186  [ next+2 char ]   (dimmed)
//
//  Gesture controls (handled by touch.ino):
//    Swipe LEFT/RIGHT  → previous/next range tab
//    Swipe UP/DOWN     → previous/next character
//    Tap LEFT zone     → insert current character (NEXT)
//    Tap MID zone      → delete last character   (DEL)
//    Tap RIGHT zone    → confirm password         (OK)
// ============================================================

// Layout y-constants (absolute screen coords)
#define KB_Y_SSID    (CONTENT_TOP +  2)   // y=22
#define KB_Y_PASS    (CONTENT_TOP + 14)   // y=34
#define KB_Y_SEP1    (CONTENT_TOP + 24)   // y=44
#define KB_Y_TABS    (CONTENT_TOP + 26)   // y=46
#define KB_TABS_H    12
#define KB_Y_SEP2    (CONTENT_TOP + 40)   // y=60
#define KB_Y_HINT    (CONTENT_TOP + 42)   // y=62
#define KB_Y_SEP3    (CONTENT_TOP + 52)   // y=72
#define KB_WHEEL_TOP (CONTENT_TOP + 55)   // y=75  — wheel starts here

// Wheel row heights: small (dimmed) rows vs large (current) row
#define KB_SMALL_H   20
#define KB_BIG_H     32
// Window: 2 above + current + 2 below = 5 items
// Total wheel height = 2*20 + 32 + 2*20 = 132 px
// Available from KB_WHEEL_TOP to CONTENT_TOP+CONTENT_H = 75..250 → 175 px ✓

// Short tab labels  (must match KB_RANGES order in HMS_RigConsole_v4.ino)
static const char * const KB_TAB_NAMES[KB_RANGE_COUNT] = {
  "az", "AZ", "09", "S1", "S2", "In"
};

// Helper: build a displayable label for a character
static void _kbCharLabel(char ch, char *buf, int bufLen) {
  if ((uint8_t)ch > 0x7E) {
    snprintf(buf, bufLen, "%02X", (uint8_t)ch);  // international → hex for now
  } else if (ch == ' ') {
    strlcpy(buf, "SPC", bufLen);
  } else {
    buf[0] = ch; buf[1] = '\0';
  }
}

void drawEspKbContent() {
  gfx->setTextSize(1);

  // ── SSID line ─────────────────────────────────────────────
  {
    char buf[30];
    snprintf(buf, sizeof(buf), "SSID:%.22s", kbPendingSSID);
    drawTxt(buf, 2, KB_Y_SSID, COL_DIM, COL_BG);
  }

  // ── Password field ────────────────────────────────────────
  {
    gfx->fillRect(0, KB_Y_PASS, SCREEN_W, 10, COL_BG);
    if (kbPassLen == 0) {
      drawTxt("(empty = open)", 2, KB_Y_PASS, COL_DIM, COL_BG);
    } else if (kbPassLen <= 22) {
      drawTxt(kbPass, 2, KB_Y_PASS, COL_TEXT, COL_BG);
      int cx2 = 2 + kbPassLen * 6;
      if (cx2 < SCREEN_W - 4) gfx->fillRect(cx2, KB_Y_PASS, 3, 8, COL_TEXT);
    } else {
      char buf[24];
      snprintf(buf, sizeof(buf), "...%s", kbPass + kbPassLen - 19);
      drawTxt(buf, 2, KB_Y_PASS, COL_TEXT, COL_BG);
      gfx->fillRect(2 + 22 * 6, KB_Y_PASS, 3, 8, COL_TEXT);
    }
  }

  gfx->drawFastHLine(0, KB_Y_SEP1, SCREEN_W, COL_BORDER);

  // ── Range tabs ────────────────────────────────────────────
  {
    int tabW = SCREEN_W / KB_RANGE_COUNT;   // 28 px
    for (int t = 0; t < KB_RANGE_COUNT; t++) {
      int      tx  = t * tabW;
      bool     act = (t == kbRangeIdx);
      uint16_t bg  = act ? COL_ACCENT : COL_STATUS_BG;
      uint16_t fg  = act ? (uint16_t)0x0000 : COL_DIM;
      gfx->fillRect(tx, KB_Y_TABS, tabW - 1, KB_TABS_H, bg);
      drawCentredTxt(KB_TAB_NAMES[t], tx + tabW / 2, KB_Y_TABS + 2, fg, bg);
    }
  }

  gfx->drawFastHLine(0, KB_Y_SEP2, SCREEN_W, COL_BORDER);

  // ── Hint line ─────────────────────────────────────────────
  drawCentredTxt("< range >   ^ char v", SCREEN_W / 2,
                 KB_Y_HINT, COL_DIM, COL_BG);

  gfx->drawFastHLine(0, KB_Y_SEP3, SCREEN_W, COL_BORDER);

  // ── Character wheel (5 items: -2 -1 [0] +1 +2) ───────────
  {
    const char *range = KB_RANGES[kbRangeIdx];
    int len = (int)strlen(range);
    if (len == 0) return;

    int cur = kbCharIdx;
    if (cur < 0) cur = 0;
    if (cur >= len) cur = len - 1;

    // Offsets from centre: -2, -1, 0, +1, +2
    const int offsets[5] = { -2, -1, 0, 1, 2 };
    int y = KB_WHEEL_TOP;

    for (int slot = 0; slot < 5; slot++) {
      int   off   = offsets[slot];
      bool  isCur = (off == 0);
      int   rowH  = isCur ? KB_BIG_H : KB_SMALL_H;
      int   idx   = ((cur + off) % len + len) % len;   // wraps
      char  ch    = range[idx];

      char lbl[5];
      _kbCharLabel(ch, lbl, sizeof(lbl));

      uint16_t bg = isCur ? COL_HEADER : COL_BG;
      uint16_t fg = isCur ? COL_ACCENT : COL_DIM;

      gfx->fillRect(0, y, SCREEN_W, rowH - 1, bg);
      if (isCur) {
        // Highlighted box with accent border
        gfx->drawRect(0, y, SCREEN_W, rowH - 1, COL_ACCENT);
        // Large centred label
        gfx->setTextSize(2);
        drawCentredTxt(lbl, SCREEN_W / 2, y + (rowH - 16) / 2, fg, bg);
        gfx->setTextSize(1);
      } else {
        drawCentredTxt(lbl, SCREEN_W / 2, y + (rowH - 8) / 2, fg, bg);
      }

      // Scroll arrows at the edge of the centre row
      if (slot == 1) {   // row just above current → up arrow hint
        drawTxt("^", SCREEN_W - 10, y + (rowH - 8) / 2, COL_DIM, bg);
      }
      if (slot == 3) {   // row just below current → down arrow hint
        drawTxt("v", SCREEN_W - 10, y + (rowH - 8) / 2, COL_DIM, bg);
      }

      y += rowH;
    }
  }
}


// ============================================================
//  ADVANCED SUB-MENU  (5 items, compact)
// ============================================================
void drawAdvancedMenuContent() {
  static const MenuItem items[] = {
    { "Start AP",         COL_TEXT },
    { "Show info",        COL_TEXT },
    { "Install on Pi",    COL_TEXT },
    { "Flip display",     COL_TEXT },
    { "<- Back",          COL_DIM  },
  };
  drawMenuList(items, 5, advMenuIndex, 0);
}


// ============================================================
//  AP PROFILE QUICK-PICK  (4 profiles + Back)
// ============================================================
void drawApSelectContent() {
  MenuItem items[MAX_AP_PROFILES + 1];
  static char labels[MAX_AP_PROFILES][28];
  for (int i = 0; i < MAX_AP_PROFILES; i++) {
    snprintf(labels[i], sizeof(labels[i]), "%d: %s", i + 1, apProfiles[i].name);
    items[i] = { labels[i], COL_TEXT };
  }
  items[MAX_AP_PROFILES] = { "<- Back", COL_DIM };
  drawMenuList(items, MAX_AP_PROFILES + 1, apSelectIndex, 0);
}


// ============================================================
//  ADVANCED INFO  (system diagnostics)
// ============================================================
void drawAdvInfoContent() {
  gfx->setTextSize(1);

  unsigned long s = millis() / 1000;
  unsigned long m = s / 60; s %= 60;
  unsigned long h = m / 60; m %= 60;
  char uptimeBuf[24];
  snprintf(uptimeBuf, sizeof(uptimeBuf), "%luh %02lum %02lus", h, m, s);
  char heapBuf[16];
  snprintf(heapBuf, sizeof(heapBuf), "%u B", ESP.getFreeHeap());
  char sketchBuf[20];
  snprintf(sketchBuf, sizeof(sketchBuf), "%u KB", ESP.getSketchSize() / 1024);
  char freeFsBuf[20];
  snprintf(freeFsBuf, sizeof(freeFsBuf), "%u KB", ESP.getFreeSketchSpace() / 1024);

  const char *piStatus = piConnected ? "Online" : "Offline";
  uint16_t    piCol    = piConnected ? COL_ACTIVE : COL_ERR;

  int y  = CONTENT_TOP + 4;
  int R  = 14;
  int kx = 4, vx = 68;

  drawTxt("Uptime  :", kx, y, COL_DIM, COL_BG);
  drawTxt(uptimeBuf,   vx, y, COL_TEXT, COL_BG);  y += R;

  drawTxt("Heap    :", kx, y, COL_DIM, COL_BG);
  drawTxt(heapBuf,     vx, y, COL_TEXT, COL_BG);  y += R;

  drawTxt("Pi link :", kx, y, COL_DIM, COL_BG);
  drawTxt(piStatus,    vx, y, piCol,   COL_BG);   y += R;

  drawTxt("USB port:", kx, y, COL_DIM, COL_BG);
  drawTxt(piPort,      vx, y, COL_TEXT, COL_BG);  y += R;

  drawTxt("ESP IP  :", kx, y, COL_DIM, COL_BG);
  drawTxt(espIP,       vx, y, espConnected ? COL_ACTIVE : COL_DIM, COL_BG);
  y += R;

  gfx->drawFastHLine(4, y + 2, SCREEN_W - 8, COL_BORDER);  y += 8;

  drawTxt("Sketch  :", kx, y, COL_DIM, COL_BG);
  drawTxt(sketchBuf,   vx, y, COL_TEXT, COL_BG);  y += R;

  drawTxt("Free OTA:", kx, y, COL_DIM, COL_BG);
  drawTxt(freeFsBuf,   vx, y, COL_TEXT, COL_BG);  y += R + 4;

  gfx->drawFastHLine(4, y, SCREEN_W - 8, COL_BORDER);  y += 6;
  drawTxt("Swipe L: back", 4, y, COL_DIM, COL_BG);
}


// ============================================================
//  AP ACTIVE
// ============================================================
void drawApActiveContent() {
  gfx->setTextSize(1);
  int y = CONTENT_TOP + 4;
  int R = 14;

  gfx->setTextSize(2);
  drawTxt("AP ACTIVE", 4, y, COL_ACTIVE, COL_BG);
  gfx->setTextSize(1);
  y += 22;
  gfx->drawFastHLine(4, y, SCREEN_W - 8, COL_BORDER);  y += 6;

  drawTxt("SSID:",    4, y, COL_DIM,  COL_BG);
  drawTxt(apProfiles[activeApProfile].ssid, 44, y, COL_TEXT, COL_BG);  y += R;
  drawTxt("Pass:",    4, y, COL_DIM,  COL_BG);
  drawTxt(apProfiles[activeApProfile].pass, 44, y, COL_TEXT, COL_BG);  y += R;
  drawTxt("Web:",     4, y, COL_DIM,  COL_BG);
  drawTxt(dynApIp.toString().c_str(), 44, y, COL_ACCENT, COL_BG);
  if (apNatActive()) drawTxt("NAT", SCREEN_W - 28, y, COL_ACTIVE, COL_BG);
  y += R;
  drawTxt("OTA:",     4, y, COL_DIM,  COL_BG);
  drawTxt("/update",  44, y, COL_DIM,  COL_BG);         y += R;

  gfx->drawFastHLine(4, y, SCREEN_W - 8, COL_BORDER);  y += 6;
  drawTxt("Clients:", 4, y, COL_DIM, COL_BG);
  char cl[4];
  snprintf(cl, sizeof(cl), "%d", (int)WiFi.softAPgetStationNum());
  drawTxt(cl, 64, y, COL_TEXT, COL_BG);  y += R;

  gfx->drawFastHLine(4, y, SCREEN_W - 8, COL_BORDER);  y += 6;
  drawTxt("Tap left: Close AP", 4, y, COL_DIM, COL_BG);
}


// ============================================================
//  OTA PROGRESS  (called from web_server.ino)
// ============================================================
void drawOtaScreen(int pct) {
  if (pct < 0) {
    gfx->fillScreen(COL_BG);
    gfx->setTextSize(2);
    drawCentredTxt("OTA UPDATE", SCREEN_W / 2, SCREEN_H / 2 - 36, COL_ACTIVE, COL_BG);
    gfx->setTextSize(1);
    drawCentredTxt("Do not power off!", SCREEN_W / 2, SCREEN_H / 2 - 8, COL_DIM, COL_BG);
    gfx->drawRect(10, SCREEN_H / 2 + 12, SCREEN_W - 20, 16, COL_BORDER);
  } else if (pct <= 100) {
    int barW = (int)((long)(SCREEN_W - 22) * pct / 100);
    gfx->fillRect(11, SCREEN_H / 2 + 13, SCREEN_W - 22, 14, COL_STATUS_BG);
    if (barW > 0) gfx->fillRect(11, SCREEN_H / 2 + 13, barW, 14, COL_ACCENT);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    gfx->fillRect(0, SCREEN_H / 2 + 32, SCREEN_W, 12, COL_BG);
    gfx->setTextSize(1);
    drawCentredTxt(buf, SCREEN_W / 2, SCREEN_H / 2 + 34, COL_DIM, COL_BG);
  } else {
    gfx->fillScreen(COL_BG);
    gfx->setTextSize(2);
    drawCentredTxt("DONE!", SCREEN_W / 2, SCREEN_H / 2 - 16, COL_ACTIVE, COL_BG);
    gfx->setTextSize(1);
    drawCentredTxt("Rebooting...", SCREEN_W / 2, SCREEN_H / 2 + 10, COL_DIM, COL_BG);
  }
  gfx->flush();
}

void drawOtaError(const char *msg) {
  gfx->setTextSize(1);
  drawCentredTxt(msg, SCREEN_W / 2, SCREEN_H / 2 + 48, COL_ERR, COL_BG);
  gfx->flush();
}
