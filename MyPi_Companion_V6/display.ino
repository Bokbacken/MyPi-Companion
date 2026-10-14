// ============================================================
//  display.ino — All draw functions for MyPi Companion
//
//  Screen layout (portrait 172×320):
//    y=  0.. 19   drawHeader()       — 20 px
//    y= 20..235   content area       — 216 px  (MENU_ROW_H=18)
//    y=236..299   drawStatusBar()    — 64 px
//    y=300..319   drawTouchBar()     — 20 px
//
//  Status bar (STATUS_TOP+offset):
//    +2   UART 8N1 waveform ('C')
//    +14  ESP IP address (textSize 2, narrow dots — fits 255.255.255.255)
//    +32  ESP SSID
//    +41  grey separator line (ESP above / Pi below)
//    +44  Pi <iface>: <ip>  (cycles if multiple)
//    +55  Pi SSID (wireless) / error / hint
// ============================================================

#include <esp_wifi.h>
#include <esp_wifi_ap_get_sta_list.h>   // esp_wifi_ap_get_sta_list_with_ip() (IDF 5.x)

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

// Draw an IPv4 address at the current text size with narrow dots:
// digits advance 6*size px, '.' advances 3*size px. At size 2 the
// worst case "255.255.255.255" is 12*12 + 3*6 = 162 px, so it fits
// the 172 px screen from x=4. Characters that would cross the right
// edge are not drawn.
static void drawCompactIP(const char *ip, int16_t x, int16_t y, uint16_t fg) {
  const uint8_t sz = 2;
  for (const char *p = ip; *p; p++) {
    bool dot = (*p == '.');
    int16_t adv = dot ? 3 * sz : 6 * sz;
    if (x + adv > SCREEN_W) break;
    // '.' glyph occupies font columns 1-2; shift left 1 px to centre it
    // in its narrow slot. Transparent (fg==bg) so neighbours are untouched.
    gfx->drawChar(dot ? x - 1 : x, y, *p, fg, fg);
    x += adv;
  }
}


// ============================================================
//  HEADER BAR
// ============================================================
void drawHeader(const char *title) {
  gfx->fillRect(0, 0, SCREEN_W, HDR_H, COL_HDR_BG);
  gfx->drawFastHLine(0, HDR_H - 1, SCREEN_W, COL_ACCENT);
  gfx->setTextSize(1);
  drawTxt(title, 10, 6, COL_HDR_TXT, COL_HDR_BG);
  int cx = SCREEN_W - LOGO_ICON_W - 10;   // 10 px margin for rounded corners
  int cy = (HDR_H - LOGO_ICON_H) / 2;
  drawProgmemImage(cx, cy, LOGO_ICON_W, LOGO_ICON_H, logo_icon);

  // AP running indicator (shown when AP is active but not on the AP page).
  // Placed between the title (longest ends at x≈106) and the logo (x=139)
  // instead of centred, so it never overlaps the title text.
  if (apRunning && currentState != STATE_AP_ACTIVE) {
    int ax = 118;
    int ay = HDR_H / 2 + 4;   // arcs open upwards from this dot
    // Arduino_GFX angles: 0° = 3 o'clock, clockwise → 210..330 = upper arc
    gfx->fillCircle(ax, ay, 2, COL_ACCENT);                 // dot
    gfx->fillArc(ax, ay, 6,  5, 210, 330, COL_ACCENT);      // inner arc
    gfx->fillArc(ax, ay, 10, 9, 210, 330, COL_ACCENT);      // outer arc
    // If NAT is active, add a small '+' to the right of the icon
    if (apNatActive()) {
      drawTxt("+", ax + 11, HDR_H / 2 - 4, COL_ACCENT, COL_HDR_BG);
    }
  }
}


// ============================================================
//  STATUS BAR  — 64 px: waveform, ESP IP/SSID, separator, Pi iface/SSID
// ============================================================
void drawStatusBar() {
  gfx->fillRect(0, STATUS_TOP, SCREEN_W, STATUS_H, COL_STATUS_BG);

  // ── UART 8N1 waveform for 0x43 ('C', Companion) ──────────
  // start, 8 data bits LSB first, stop; 10 bits x 17 px = 170 px
  static const uint8_t WAVE_BITS[10] = {0, 1, 1, 0, 0, 0, 0, 1, 0, 1};
  const int BIT_W = 17;
  const int yHi = STATUS_TOP + 2;
  const int yLo = STATUS_TOP + 12;
  int prevY = WAVE_BITS[0] ? yHi : yLo;
  for (int i = 0; i < 10; i++) {
    int y  = WAVE_BITS[i] ? yHi : yLo;
    int x0 = i * BIT_W;
    if (y != prevY)
      gfx->drawFastVLine(x0, min(y, prevY), abs(y - prevY) + 1, 0x07E0);
    gfx->drawFastHLine(x0, y, BIT_W, 0x07E0);
    prevY = y;
  }

  // ── ESP IP address (textSize 2) ───────────────────────────
  int yEspIP   = STATUS_TOP + SB_ESP_IP;
  int yEspSSID = STATUS_TOP + SB_ESP_SSID;
  gfx->setTextSize(2);
  if (espConnected) {
    drawCompactIP(espIP, 4, yEspIP, COL_ACTIVE);
  } else {
    drawTxt("no Wi-Fi", 2, yEspIP, COL_DIM, COL_STATUS_BG);
  }

  // ── ESP SSID (textSize 1) ─────────────────────────────────
  gfx->setTextSize(1);
  if (espConnected) {
    char ssBuf[29];
    truncate(ssBuf, espSSID, 28);        // max 28 chars x 6 px = 168 px
    drawTxt(ssBuf, 4, yEspSSID, COL_DIM, COL_STATUS_BG);
  }

  // ── Separator between ESP and Pi sections ─────────────────
  gfx->drawFastHLine(4, STATUS_TOP + SB_SEP, SCREEN_W - 8, COL_SEP);

  // ── Pi interface + IP (textSize 1) ───────────────────────
  int yPiIface = STATUS_TOP + SB_PI_IFACE;
  int yPiSSID  = STATUS_TOP + SB_PI_SSID;
  gfx->setTextSize(1);

  if (!piConnected) {
    drawTxt("Pi: offline", 4, yPiIface, COL_ERR, COL_STATUS_BG);
    drawTxt("Connect Pi via USB", 4, yPiSSID, COL_DIM, COL_STATUS_BG);
  } else if (piIfaceCount > 0) {
    int idx = piIfaceIdx % piIfaceCount;
    // "Pi wlan0: 192.168.x.x" — snprintf truncates to 28 chars
    char piBuf[29];
    snprintf(piBuf, sizeof(piBuf), "Pi %s: %s",
             piIfaces[idx].name, piIfaces[idx].ip);
    drawTxt(piBuf, 4, yPiIface, COL_ACTIVE, COL_STATUS_BG);

    // SSID line: error > info > SSID for wireless iface
    if (hasError) {
      char errBuf[29];
      truncate(errBuf, lastErrMsg, 28);
      drawTxt(errBuf, 4, yPiSSID, COL_ERR, COL_STATUS_BG);
    } else if (hasInfo) {
      char infoBuf[29];
      truncate(infoBuf, infoMsg, 28);
      drawTxt(infoBuf, 4, yPiSSID, COL_ACCENT, COL_STATUS_BG);
    } else if (strncmp(piIfaces[idx].name, "wl", 2) == 0) {
      char ssBuf[29];
      truncate(ssBuf, activeSSID, 28);
      drawTxt(ssBuf, 4, yPiSSID, COL_DIM, COL_STATUS_BG);
    }
    // Ethernet etc.: SSID line stays blank (cleared by fillRect)
  } else {
    // piConnected but no iface data yet — legacy fallback
    drawTxt("Pi: ", 4, yPiIface, COL_DIM, COL_STATUS_BG);
    char ssBuf[25];
    truncate(ssBuf, activeSSID, 24);     // starts at x=28: 24 x 6 = 144 px
    drawTxt(ssBuf, 28, yPiIface, COL_ACTIVE, COL_STATUS_BG);
    if (hasError) {
      char errBuf[29];
      truncate(errBuf, lastErrMsg, 28);
      drawTxt(errBuf, 4, yPiSSID, COL_ERR, COL_STATUS_BG);
    } else if (hasInfo) {
      char infoBuf[29];
      truncate(infoBuf, infoMsg, 28);
      drawTxt(infoBuf, 4, yPiSSID, COL_ACCENT, COL_STATUS_BG);
    } else {
      char ipBuf[29];
      truncate(ipBuf, activeIP, 28);
      drawTxt(ipBuf, 4, yPiSSID, COL_DIM, COL_STATUS_BG);
    }
  }
  gfx->setTextSize(1);   // other draw code expects size 1
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

// Short tab labels  (must match KB_RANGES order in MyPi_Companion_V6.ino)
static const char * const KB_TAB_NAMES[KB_RANGE_COUNT] = {
  "az", "AZ", "09", "S1", "S2", "I1", "I2"
};

// Characters missing from the CP437 GFX font, drawn as custom 5x8 bitmaps.
// rows[r] bit4 = leftmost pixel; row 7 is the descender row.
struct KbGlyph { uint8_t code; uint8_t rows[8]; };
static const KbGlyph KB_GLYPHS[] = {
  { 0xF8, { 0x00, 0x00, 0x0E, 0x13, 0x15, 0x19, 0x0E, 0x00 } },  // ø
  { 0xD8, { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E, 0x00 } },  // Ø
  { 0xF0, { 0x06, 0x0E, 0x01, 0x0F, 0x11, 0x11, 0x0E, 0x00 } },  // ð
  { 0xD0, { 0x0E, 0x09, 0x09, 0x1D, 0x09, 0x09, 0x0E, 0x00 } },  // Ð
  { 0xFE, { 0x10, 0x10, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10 } },  // þ
  { 0xDE, { 0x10, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x00 } },  // Þ
  { 0x9C, { 0x00, 0x00, 0x0A, 0x15, 0x17, 0x14, 0x0F, 0x00 } },  // œ
  { 0xFD, { 0x02, 0x04, 0x11, 0x11, 0x0F, 0x01, 0x0E, 0x00 } },  // ý
  { 0xE3, { 0x0D, 0x16, 0x0E, 0x01, 0x0F, 0x11, 0x0F, 0x00 } },  // ã
  { 0xF5, { 0x0D, 0x16, 0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00 } },  // õ
  { 0x80, { 0x07, 0x08, 0x1E, 0x08, 0x1E, 0x08, 0x07, 0x00 } },  // €
  { 0xA4, { 0x00, 0x11, 0x0E, 0x0A, 0x0E, 0x11, 0x00, 0x00 } },  // ¤
};

// Returns the 8 bitmap rows for ch, or nullptr if the CP437 font has it.
// (Returns a plain pointer: Arduino's auto-prototypes can't see KbGlyph.)
static const uint8_t *_kbCustomGlyph(char ch) {
  for (const KbGlyph &g : KB_GLYPHS) if (g.code == (uint8_t)ch) return g.rows;
  return nullptr;
}

// Map a Windows-1252 byte (as stored in KB_RANGES / kbPass) to the glyph
// index of the built-in CP437 GFX font. Bytes with a custom glyph map to a
// blank so _kbDrawCustomGlyphs() can paint them on top afterwards.
static char _latin1ToCp437(char ch) {
  if (_kbCustomGlyph(ch)) return ' ';
  switch ((uint8_t)ch) {
    case 0xE5: return '\x86';  case 0xE4: return '\x84';  case 0xF6: return '\x94';  // å ä ö
    case 0xC5: return '\x8F';  case 0xC4: return '\x8E';  case 0xD6: return '\x99';  // Å Ä Ö
    case 0xE6: return '\x91';  case 0xC6: return '\x92';                             // æ Æ
    case 0xFC: return '\x81';  case 0xDC: return '\x9A';  case 0xDF: return '\xE1';  // ü Ü ß
    case 0xE9: return '\x82';  case 0xC9: return '\x90';  case 0xE8: return '\x8A';  // é É è
    case 0xEA: return '\x88';  case 0xEB: return '\x89';  case 0xE0: return '\x85';  // ê ë à
    case 0xE2: return '\x83';  case 0xE7: return '\x87';  case 0xC7: return '\x80';  // â ç Ç
    case 0xFB: return '\x96';  case 0xF9: return '\x97';  case 0xFA: return '\xA3';  // û ù ú
    case 0xEE: return '\x8C';  case 0xEF: return '\x8B';  case 0xF4: return '\x93';  // î ï ô
    case 0xFF: return '\x98';  case 0xE1: return '\xA0';  case 0xED: return '\xA1';  // ÿ á í
    case 0xF3: return '\xA2';  case 0xF1: return '\xA4';  case 0xD1: return '\xA5';  // ó ñ Ñ
    case 0xEC: return '\x8D';  case 0xF2: return '\x95';                             // ì ò
    case 0xBF: return '\xA8';  case 0xA1: return '\xAD';                             // ¿ ¡
    case 0xA3: return '\x9C';  case 0xA5: return '\x9D';  case 0xA7: return '\x15';  // £ ¥ §
    default:   return ch;
  }
}

// Paint custom glyphs for a string already printed with _latin1ToCp437Str()
// at (x, y). Each character cell is 6*size px wide.
static void _kbDrawCustomGlyphs(const char *s, int16_t x, int16_t y,
                                uint8_t size, uint16_t fg) {
  for (int i = 0; s[i]; i++) {
    const uint8_t *rows = _kbCustomGlyph(s[i]);
    if (!rows) continue;
    int16_t cx = x + i * 6 * size;
    for (int r = 0; r < 8; r++)
      for (int c = 0; c < 5; c++)
        if (rows[r] & (0x10 >> c))
          gfx->fillRect(cx + c * size, y + r * size, size, size, fg);
  }
}

// Copy a Latin-1 string into buf with each byte mapped for the CP437 font
static void _latin1ToCp437Str(const char *src, char *buf, int bufLen) {
  int i = 0;
  for (; src[i] && i < bufLen - 1; i++) buf[i] = _latin1ToCp437(src[i]);
  buf[i] = '\0';
}

// Helper: build a displayable label for a character
static void _kbCharLabel(char ch, char *buf, int bufLen) {
  if (ch == ' ') {
    strlcpy(buf, "SPC", bufLen);
  } else {
    buf[0] = _latin1ToCp437(ch); buf[1] = '\0';   // GFX font is CP437 (incl. >0x7E)
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
      char disp[24];
      _latin1ToCp437Str(kbPass, disp, sizeof(disp));
      drawTxt(disp, 2, KB_Y_PASS, COL_TEXT, COL_BG);
      _kbDrawCustomGlyphs(kbPass, 2, KB_Y_PASS, 1, COL_TEXT);
      int cx2 = 2 + kbPassLen * 6;
      if (cx2 < SCREEN_W - 4) gfx->fillRect(cx2, KB_Y_PASS, 3, 8, COL_TEXT);
    } else {
      char raw[24], buf[24];
      snprintf(raw, sizeof(raw), "...%s", kbPass + kbPassLen - 19);
      _latin1ToCp437Str(raw, buf, sizeof(buf));
      drawTxt(buf, 2, KB_Y_PASS, COL_TEXT, COL_BG);
      _kbDrawCustomGlyphs(raw, 2, KB_Y_PASS, 1, COL_TEXT);
      gfx->fillRect(2 + 22 * 6, KB_Y_PASS, 3, 8, COL_TEXT);
    }
  }

  gfx->drawFastHLine(0, KB_Y_SEP1, SCREEN_W, COL_BORDER);

  // ── Range tabs ────────────────────────────────────────────
  {
    int tabW = SCREEN_W / KB_RANGE_COUNT;   // 24 px
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
      if (_kbCustomGlyph(ch)) {   // single-char label: cell centred at SCREEN_W/2
        uint8_t sz = isCur ? 2 : 1;
        const char one[2] = { ch, '\0' };
        _kbDrawCustomGlyphs(one, SCREEN_W / 2 - 3 * sz,
                            y + (rowH - 8 * sz) / 2, sz, fg);
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
//  ADVANCED SUB-MENU  (7 items, compact)
// ============================================================
void drawAdvancedMenuContent() {
  // GUI install confirmation overlay replaces the menu while pending
  if (advGuiConfirmPending) {
    gfx->fillRect(0, CONTENT_TOP, SCREEN_W, CONTENT_H, COL_BG);
    int y = CONTENT_TOP + 20;
    gfx->setTextSize(2);
    drawTxt("Confirm:", 4, y, COL_ACCENT, COL_BG);
    gfx->setTextSize(1);
    y += 28;
    drawTxt("Install GUI-Pi?", 4, y, COL_TEXT, COL_BG);
    y += 20;
    drawTxt("LEFT=Cancel  RIGHT=Yes", 4, y, COL_DIM, COL_BG);
    return;
  }

  // Boot AP: per-profile flag (none / profile / profile+Hide)
  char     apBootBuf[22];
  int      bs        = apBootSlot();
  uint16_t apBootCol = (bs >= 0) ? COL_ACTIVE : COL_DIM;
  if (bs < 0) {
    strlcpy(apBootBuf, "Boot AP: none", sizeof(apBootBuf));
  } else if (apProfiles[bs].hideOnBoot) {
    snprintf(apBootBuf, sizeof(apBootBuf), "Boot:%.10s+Hide", apProfiles[bs].name);
  } else {
    snprintf(apBootBuf, sizeof(apBootBuf), "Boot AP:%.11s", apProfiles[bs].name);
  }

  const MenuItem items[ADV_MENU_COUNT] = {
    { "Start AP",         COL_TEXT },
    { "Show info",        COL_TEXT },
    { "Install CLI-Pi",   COL_TEXT },
    { "Install GUI-Pi",   COL_TEXT },
    { "Flip display",     COL_TEXT },
    { apBootBuf,          apBootCol   },
    { "<- Back",          COL_DIM  },
  };
  drawMenuList(items, ADV_MENU_COUNT, advMenuIndex, 0);
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
//  AP BOOT PROFILE SELECTOR  (None + 4 profiles)
// ============================================================
void drawApBootSelectContent() {
  // Items: 0 = None, 1..MAX_AP_PROFILES = profile slot i-1
  const int total = MAX_AP_PROFILES + 1;
  MenuItem items[MAX_AP_PROFILES + 1];
  static char labels[MAX_AP_PROFILES][28];
  int bootSlot = apBootSlot();   // -1 if none

  items[0] = { "None", (bootSlot < 0) ? COL_ACTIVE : COL_DIM };
  for (int i = 0; i < MAX_AP_PROFILES; i++) {
    snprintf(labels[i], sizeof(labels[i]), "%d: %s", i + 1, apProfiles[i].name);
    items[i + 1] = { labels[i], (bootSlot == i) ? COL_ACTIVE : COL_TEXT };
  }
  drawMenuList(items, total, apBootSelectIdx, 0);
}


// ============================================================
//  ADVANCED INFO  (system diagnostics)
// ============================================================
// Reformat compiler __DATE__ "Oct 13 2026" → "13 Oct 2026"
static void _fmtBuildDate(char *buf, int bufLen) {
  // __DATE__ is "MMM DD YYYY" — day may be space-padded
  char mon[4]; int day; char year[5];
  if (sscanf(__DATE__, "%3s %d %4s", mon, &day, year) == 3) {
    snprintf(buf, bufLen, "%d %s %s", day, mon, year);
  } else {
    strlcpy(buf, __DATE__, bufLen);
  }
}

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

  char dateBuf[16];
  _fmtBuildDate(dateBuf, sizeof(dateBuf));
  char verBuf[32];
  snprintf(verBuf, sizeof(verBuf), "v%s  %s", FW_VERSION, dateBuf);
  drawTxt("Version :", kx, y, COL_DIM, COL_BG);
  drawTxt(verBuf,      vx, y, COL_ACCENT, COL_BG);  y += R;

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
  drawTxt("L: back   R: Pi info", 4, y, COL_DIM, COL_BG);
}

// ── Raspberry Pi page (data from STATUS:SYS) ─────────────────────
#define INFO_VAL_CHARS 17   // (SCREEN_W - 68 - 2) / 6 px per glyph

static void _infoRow(int &y, const char *k, const char *v, uint16_t col) {
  char buf[INFO_VAL_CHARS + 1];
  strlcpy(buf, (v && *v) ? v : "---", sizeof(buf));
  drawTxt(k,   4,  y, COL_DIM, COL_BG);
  drawTxt(buf, 68, y, (v && *v) ? col : COL_DIM, COL_BG);
  y += 14;
}

// "Raspberry Pi 4 Model B Rev 1.4" → "Pi 4 B r1.4"
static void _shortModel(const char *in, char *out, size_t len) {
  String m(in);
  m.replace("Raspberry Pi", "Pi");
  m.replace(" Model ", " ");
  m.replace(" Plus", "+");
  m.replace(" Rev ", " r");
  strlcpy(out, m.c_str(), len);
}

void drawAdvInfoPiContent() {
  gfx->setTextSize(1);
  int y = CONTENT_TOP + 4;

  if (piSysCount == 0) {
    drawTxt("No Pi system info yet.", 4, y, COL_TEXT, COL_BG);  y += 18;
    if (piConnected) {
      drawTxt("The Pi daemon is too",   4, y, COL_DIM, COL_BG);  y += 12;
      drawTxt("old - reinstall it with", 4, y, COL_DIM, COL_BG); y += 12;
      drawTxt("Advanced > Install.",     4, y, COL_DIM, COL_BG); y += 12;
    } else {
      drawTxt("Pi link is offline.",    4, y, COL_ERR, COL_BG);  y += 12;
    }
  } else {
    char buf[40];
    // Values go stale when the link drops — show them dimmed
    bool fresh = piConnected && (millis() - piSysMs) < 20000UL;
    uint16_t txt = fresh ? COL_TEXT : COL_DIM;

    _infoRow(y, "Host    :", piSysGet("host"), fresh ? COL_ACCENT : COL_DIM);
    _shortModel(piSysGet("model"), buf, sizeof(buf));
    _infoRow(y, "Model   :", buf, txt);
    // "bookworm 64-bit" fits better than PRETTY_NAME
    const char *code = piSysGet("code");
    const char *arch = piSysGet("arch");
    if (*code) {
      bool b64 = strstr(arch, "64") != nullptr;
      snprintf(buf, sizeof(buf), "%s%s", code, *arch ? (b64 ? " 64-bit" : " 32-bit") : "");
    } else {
      strlcpy(buf, piSysGet("os"), sizeof(buf));
    }
    _infoRow(y, "OS      :", buf, txt);
    _infoRow(y, "Kernel  :", piSysGet("kernel"), txt);
    const char *dv = piSysGet("daemon");
    if (*dv) snprintf(buf, sizeof(buf), "v%s", dv); else buf[0] = 0;
    _infoRow(y, "Daemon  :", buf, txt);

    gfx->drawFastHLine(4, y + 2, SCREEN_W - 8, COL_BORDER);  y += 8;

    // Uptime "3d 04h 12m"
    long up = atol(piSysGet("up"));
    if (*piSysGet("up")) {
      if (up >= 86400) snprintf(buf, sizeof(buf), "%ldd %02ldh %02ldm", up / 86400, (up / 3600) % 24, (up / 60) % 60);
      else             snprintf(buf, sizeof(buf), "%ldh %02ldm", up / 3600, (up / 60) % 60);
    } else buf[0] = 0;
    _infoRow(y, "Uptime  :", buf, txt);

    const char *tc = piSysGet("temp");
    float t = atof(tc);
    if (*tc) snprintf(buf, sizeof(buf), "%s C", tc); else buf[0] = 0;
    _infoRow(y, "CPU temp:", buf, !fresh ? COL_DIM : t >= 80 ? COL_ERR : t >= 70 ? COL_ACCENT : COL_TEXT);

    _infoRow(y, "Load 1m :", piSysGet("load"), txt);
    const char *mem = piSysGet("mem");
    if (*mem) snprintf(buf, sizeof(buf), "%s MB", mem); else buf[0] = 0;
    _infoRow(y, "RAM     :", buf, txt);
    const char *dk = piSysGet("disk");
    if (*dk) snprintf(buf, sizeof(buf), "%s GB", dk); else buf[0] = 0;
    _infoRow(y, "Disk /  :", buf, txt);

    // Throttle flags: bit0 under-voltage now, bits1-3 capped/throttled/
    // soft-limit now, bit16 under-voltage since boot, bits17-19 since boot
    const char *th = piSysGet("thr");
    uint16_t pc = COL_ACTIVE;
    if (*th) {
      unsigned long f = strtoul(th, nullptr, 16);
      if      (f & 0x1)     { strlcpy(buf, "UNDERVOLTAGE!", sizeof(buf)); pc = COL_ERR; }
      else if (f & 0xE)     { strlcpy(buf, "THROTTLED!",    sizeof(buf)); pc = COL_ERR; }
      else if (f & 0x10000) { strlcpy(buf, "UV since boot", sizeof(buf)); pc = COL_ACCENT; }
      else if (f & 0xE0000) { strlcpy(buf, "Thr since boot", sizeof(buf)); pc = COL_ACCENT; }
      else                    strlcpy(buf, "OK", sizeof(buf));
    } else buf[0] = 0;
    _infoRow(y, "Power   :", buf, fresh ? pc : COL_DIM);

    const char *ws = piSysGet("wifi");
    if (*ws) snprintf(buf, sizeof(buf), "%s dBm", ws); else buf[0] = 0;
    int dbm = atoi(ws);
    _infoRow(y, "Wi-Fi   :", buf, !fresh ? COL_DIM : dbm <= -80 ? COL_ERR : dbm <= -70 ? COL_ACCENT : COL_TEXT);

    const char *ntp = piSysGet("ntp");
    bool sync = strcmp(ntp, "yes") == 0;
    _infoRow(y, "Clock   :", *ntp ? (sync ? "NTP synced" : "not synced") : "",
             !fresh ? COL_DIM : sync ? COL_TEXT : COL_ACCENT);
  }

  y = CONTENT_TOP + CONTENT_H - 14;
  gfx->drawFastHLine(4, y - 4, SCREEN_W - 8, COL_BORDER);
  drawTxt("L: back   R: ESP info", 4, y, COL_DIM, COL_BG);
}


// ============================================================
//  AP ACTIVE
// ============================================================

// OUI (first 3 MAC bytes) → manufacturer, or nullptr if unknown
const char* _ouiLookup(const uint8_t *mac) {   // also used by web_server.ino
  uint32_t oui = ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | mac[2];
  // Raspberry Pi Foundation / Trading
  if (oui == 0xB827EB || oui == 0xDCA632 || oui == 0xE45F01 ||
      oui == 0xD83ADD || oui == 0x2CCF67 || oui == 0x28CDC1) return "Raspberry Pi";
  // Apple
  if (oui == 0xA4C3F0 || oui == 0x3C0630 || oui == 0x8C8590 ||
      oui == 0xA851AB || oui == 0x003EE1 || oui == 0x60F81D ||
      oui == 0x186590) return "Apple";
  // Intel
  if (oui == 0x001B21 || oui == 0x8CEC4B || oui == 0xA434D9 ||
      oui == 0x54271E || oui == 0x340286) return "Intel";
  // Samsung
  if (oui == 0x002637 || oui == 0xCC07AB || oui == 0x7840E4) return "Samsung";
  // Google
  if (oui == 0xF4F5D8 || oui == 0x546009) return "Google";
  return nullptr;   // unknown — caller shows short MAC
}

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
  y += R;
  if (apProfiles[activeApProfile].nat) {
    drawTxt("NAT:", 4, y, COL_DIM, COL_BG);
    drawTxt(apNatStatus(), 44, y, apNatActive() ? COL_ACTIVE : COL_ACCENT, COL_BG);
    y += R;
  }
  drawTxt("OTA:",     4, y, COL_DIM,  COL_BG);
  drawTxt("/update",  44, y, COL_DIM,  COL_BG);         y += R;

  gfx->drawFastHLine(4, y, SCREEN_W - 8, COL_BORDER);  y += 6;
  // ── Client list: MAC from the Wi-Fi driver, IP from DHCP server/ARP ──
  {
    static wifi_sta_list_t        wsl;   // static: ~0.5 KB, keep off the stack
    static wifi_sta_mac_ip_list_t ipl;
    memset(&wsl, 0, sizeof(wsl));
    memset(&ipl, 0, sizeof(ipl));
    bool haveIp = false;
    if (esp_wifi_ap_get_sta_list(&wsl) == ESP_OK)
      haveIp = (esp_wifi_ap_get_sta_list_with_ip(&wsl, &ipl) == ESP_OK);
    int num = wsl.num;

    char hdr[24];
    snprintf(hdr, sizeof(hdr), "Clients (%d):", num);
    drawTxt(hdr, 4, y, COL_DIM, COL_BG);  y += R;

    int show = num < 4 ? num : 4;      // screen space: max 4 rows
    for (int i = 0; i < show; i++) {
      const uint8_t *mac = wsl.sta[i].mac;
      char ipStr[6] = ".?";
      if (haveIp && ipl.sta[i].ip.addr != 0)
        snprintf(ipStr, sizeof(ipStr), ".%d", (int)esp_ip4_addr4(&ipl.sta[i].ip));

      const char *mfr = _ouiLookup(mac);
      char line[32];
      if (mfr) {
        snprintf(line, sizeof(line), "%-4s %.20s", ipStr, mfr);
      } else {
        snprintf(line, sizeof(line), "%-4s %02x:%02x:%02x",
                 ipStr, mac[3], mac[4], mac[5]);
      }
      drawTxt(line, 4, y, COL_TEXT, COL_BG);  y += R;
    }
    if (num == 0) {
      drawTxt("(none)", 4, y, COL_DIM, COL_BG);  y += R;
    }
  }

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
