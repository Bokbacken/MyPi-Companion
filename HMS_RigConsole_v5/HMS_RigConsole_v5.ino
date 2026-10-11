// ============================================================
//  HMS Rig Console — Firmware v5
//  Waveshare ESP32-S3-Touch-LCD-1.47
//  Portrait 172×320 | JD9853 (via Arduino_GFX) | AXS5106L touch
// ============================================================
//  Capacitive touch zones (bottom of screen, y >= TOUCH_TOP):
//    Left  half  (x <  TOUCH_X_MID)  →  NEXT
//    Right half  (x >= TOUCH_X_MID)  →  SELECT
//
//  Serial protocol (115200 baud, USB CDC → /dev/ttyACM0 on Pi)
//    ESP32→Pi : HELLO | CMD:GET_STATUS | CMD:CONNECT:<p>
//               CMD:START_AP | CMD:STOP_AP | CMD:ADDWIFI:...
//    Pi→ESP32 : ACK | STATUS:SSID:... | STATUS:IP:... | PROFILES:...
//               OK:CONNECT:... | ERR:CONNECT:... | TERM:OUT:...
//
//  Multi-file layout:
//    config.h              — pins, geometry, colours, structs, enum
//    jd9853_init.h         — JD9853-specific register init
//    touch_axs5106l.h      — AXS5106L I2C touch driver (header-only)
//    HMS_RigConsole_v5.ino — includes, globals, setup, loop, splash
//    display.ino           — all draw*() functions  (Arduino_GFX)
//    touch.ino             — handleTouch, dispatchAction, pollWifiScan
//    protocol.ino          — handleSerial, processMsg, wsEvent
//    nvs_profiles.ino      — loadNvsProfiles, saveNvsSlot, deleteNvsSlot
//    web_server.ino        — HTTP handlers, startWebServer, startAP/stopAP
//    wifi_manager.ino      — wifiAutoConnect, wifiConnectSlot, espSSID/IP
//
//  Arduino IDE settings:
//    Board       : ESP32S3 Dev Module
//    USB CDC     : Enabled
//    PSRAM       : OPI PSRAM
//    Flash size  : 16 MB  (or 8 MB)
//    Upload speed: 921600
// ============================================================

#include "config.h"

// --- Library include order matters (FS before WebServer) ---
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <FFat.h>
using namespace fs;
#include <WebServer.h>
#include <Preferences.h>

#include "jd9853_init.h"
#include "touch_axs5106l.h"
#include "canvas_psram.h"    // Arduino_Canvas_PSRAM — framebuffer in OPI-PSRAM
// hid.ino  — USBHIDKeyboard, hidBegin(), hidInstallOnPi()
// msc.ino  — USBMSC, mscBegin()   (RIGSETUP built from /rigsetup/ by fat12_builder.ino)
// All USB.h / USBMSC.h includes live in their respective .ino files.

#include "xterm_js.h"
#include "xterm_css.h"
#include "terminal_html.h"
#include "profiles_html.h"
#include "hms_logo_splash.h"       // 110×64 px
#include "anybus_logo_splash.h"    // 130×26 px
#include "hms_logo_corner.h"       // 32×19 px

// -----------------------------------------------------------
//  GFX objects  (heap-allocated; gfx is the canvas)
// -----------------------------------------------------------
Arduino_DataBus *bus   = nullptr;
Arduino_GFX    *panel  = nullptr;
Arduino_Canvas_PSRAM *gfx    = nullptr;

// -----------------------------------------------------------
//  AP objects
// -----------------------------------------------------------
const char*     AP_SSID = AP_SSID_STR;
const char*     AP_PASS = AP_PASS_STR;
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GW(192, 168, 4, 1);
const IPAddress AP_SN(255, 255, 255, 0);

// -----------------------------------------------------------
//  Web server / WebSocket objects
// -----------------------------------------------------------
WebServer        server(80);
WebSocketsServer wsServer(81);
int              termWsClient = -1;

// -----------------------------------------------------------
//  Dynamic profiles (from Pi via serial)
// -----------------------------------------------------------
char  dynProfiles[MAX_WIFI_PROFILES][MAX_PROFILE_LEN];
int   numDynProfiles  = 0;
char  activeSSID[MAX_PROFILE_LEN] = "---";  // actual Wi-Fi SSID  (for display)
char  activeConn[MAX_PROFILE_LEN] = "---";  // NM connection name (for CMD:CONNECT)
char  activeIP[24]                = "---";
bool  piConnected                 = false;
char  lastErrMsg[64]              = "";
bool  hasError                    = false;

// -----------------------------------------------------------
//  Menu indices
// -----------------------------------------------------------
int   menuIndex    = 0;
int   piMenuIndex  = 0;
int   nvsMenuIndex = 0;

// -----------------------------------------------------------
//  ESP Wi-Fi state  (set by wifi_manager.ino)
//  Declared here (main .ino = first in compile order) so all
//  other .ino files can see them without extern declarations.
// -----------------------------------------------------------
char  espSSID[33]  = "---";
char  espIP[24]    = "---";
bool  espConnected = false;
int   espMenuIndex = 0;
int   espActiveSlot = -1;   // NVS slot ESP is connected on (-1=none)

// Pending Install-on-Pi request — set by web handler (HTTP task),
// cleared + executed in loop() (Arduino task).
volatile bool pendingPiInstall = false;

// -----------------------------------------------------------
//  KB ranges — 6 character groups (visible to all .ino files)
// -----------------------------------------------------------
const char * const KB_RANGES[KB_RANGE_COUNT] = {
  "abcdefghijklmnopqrstuvwxyz",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "0123456789",
  "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
  "()[]{}<>|~ ",
  "\xe5\xe4\xf6\xc5\xc4\xd6\xe9\xe8\xea\xeb\xe0\xe2\xe7\xfc\xfb\xf9\xfa\xee\xef\xf4"
};

int  kbRangeIdx = 0;
int  kbCharIdx  = 0;
char kbPass[65] = "";
int  kbPassLen  = 0;
char kbPendingSSID[33] = "";

// -----------------------------------------------------------
//  ESP Wi-Fi scan state  (filled by pollWifiScan in touch.ino)
//  Uses ESP32's own radio — 2.4 GHz only.
// -----------------------------------------------------------
char scanSSIDs[MAX_SCAN_RESULTS][33];
int  scanCount  = 0;
int  scanIndex  = 0;
bool scanBusy   = false;

// -----------------------------------------------------------
//  Pi Wi-Fi scan state  (filled by SCAN_RESULT: messages in protocol.ino)
//  Pi runs nmcli — sees both 2.4 GHz AND 5 GHz.
// -----------------------------------------------------------
char piScanSSIDs[MAX_SCAN_RESULTS][33];
int  piScanCount  = 0;
int  piScanIndex  = 0;
bool piScanBusy   = false;

// -----------------------------------------------------------
//  NVS profiles  (shared pool — used for both ESP Wi-Fi + Pi push)
// -----------------------------------------------------------
NvsProfile nvsProfiles[MAX_NVS_SLOTS];

// -----------------------------------------------------------
//  Pi profiles buffer  (filled on PROFILES_FULL:)
// -----------------------------------------------------------
PiProfile piProfiles[MAX_PI_PROFILES];
int  numPiProfiles   = 0;
bool piProfilesFresh = false;

// -----------------------------------------------------------
//  Advanced sub-menu
// -----------------------------------------------------------
int  advMenuIndex   = 0;            // 0=Start AP  1=Show info  2=Install  3=Flip  4=Back
bool displayFlipped = false;        // 180° rotation — persisted in NVS key "flip"
char piPort[32]     = "---";        // USB port reported by Pi (strips /dev/)

// -----------------------------------------------------------
//  Serial protocol
// -----------------------------------------------------------
static String        serialBuf    = "";
static unsigned long lastPollMs   = 0;
static unsigned long lastStatusMs = 0;

// -----------------------------------------------------------
//  State machine
// -----------------------------------------------------------
State currentState = STATE_STATUS;


// ============================================================
//  PROGMEM image helper
//  NOTE: Images generated for TFT_eSPI are stored in big-endian
//  byte order. Arduino_GFX expects native uint16_t (little-endian
//  on ESP32). If colours look wrong, enable the bswap line below.
// ============================================================
void drawProgmemImage(int16_t x, int16_t y, int16_t w, int16_t h,
                      const uint16_t *img) {
  for (int16_t row = 0; row < h; row++) {
    for (int16_t col = 0; col < w; col++) {
      uint16_t px = pgm_read_word(&img[row * w + col]);
      // If colours appear inverted, uncomment the next line:
      // px = __builtin_bswap16(px);
      gfx->drawPixel(x + col, y + row, px);
    }
  }
}

// Like drawProgmemImage but treats transpColor as transparent —
// pixels matching that colour are not drawn, letting the background show through.
// Used for splash logos (black bg 0x0000) on the white splash screen.
void drawProgmemImageTransp(int16_t x, int16_t y, int16_t w, int16_t h,
                             const uint16_t *img, uint16_t transpColor) {
  for (int16_t row = 0; row < h; row++) {
    for (int16_t col = 0; col < w; col++) {
      uint16_t px = pgm_read_word(&img[row * w + col]);
      if (px == transpColor) continue;   // skip background
      gfx->drawPixel(x + col, y + row, px);
    }
  }
}


// ============================================================
//  SPLASH SCREEN
//
//  Layout (white background, portrait 172×320):
//    Upper half (y 0..159)  — HMS logo + "HMS Networks"
//    Lower half (y 160..319)— Anybus logo + "Rig Console v5"
//
//  Sequence:
//    1. Start async Wi-Fi scan (non-blocking) immediately.
//    2. Show HMS logo on upper half → flush → delay 1 s.
//    3. Add Anybus logo on lower half → flush.
//    4. Poll until scan done (cap 3 s) → menu appears.
//
//  This overlaps ~1.5 s of scan time with the splash display,
//  cutting perceived startup time significantly.
// ============================================================

// White splash background (RGB565 0xFFFF)
#define COL_SPLASH_BG  ((uint16_t)0xFFFF)
// Dark text colour for white splash
#define COL_SPLASH_TXT ((uint16_t)0x0000)

// Draw HMS logo on the upper half of a white screen.
static void _splashTop() {
  // Centre 110×64 logo in the upper 160 px
  int x1 = (SCREEN_W - HMS_LOGO_SPLASH_W) / 2;
  int y1 = (160 - HMS_LOGO_SPLASH_H) / 2;
  drawProgmemImageTransp(x1, y1, HMS_LOGO_SPLASH_W, HMS_LOGO_SPLASH_H, hms_logo_splash, 0x0000);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_SPLASH_TXT, COL_SPLASH_BG);
  int16_t bx, by; uint16_t tw, th;
  gfx->getTextBounds("HMS Networks", 0, 0, &bx, &by, &tw, &th);
  gfx->setCursor((SCREEN_W - tw) / 2, y1 + HMS_LOGO_SPLASH_H + 6);
  gfx->print("HMS Networks");
}

// Draw Anybus logo on the lower half of the same white screen.
static void _splashBottom() {
  // Centre 130×26 logo in the lower 160 px (starting at y=160)
  int x2 = (SCREEN_W - ANYBUS_LOGO_SPLASH_W) / 2;
  int y2 = 160 + (160 - ANYBUS_LOGO_SPLASH_H) / 2;
  drawProgmemImageTransp(x2, y2, ANYBUS_LOGO_SPLASH_W, ANYBUS_LOGO_SPLASH_H, anybus_logo_splash, 0x0000);

  const char *lbl = "Rig Console  v5";
  gfx->setTextSize(1);
  gfx->setTextColor(COL_SPLASH_TXT, COL_SPLASH_BG);
  int16_t bx, by; uint16_t tw, th;
  gfx->getTextBounds(lbl, 0, 0, &bx, &by, &tw, &th);
  gfx->setCursor((SCREEN_W - tw) / 2, y2 + ANYBUS_LOGO_SPLASH_H + 8);
  gfx->print(lbl);
}


// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.setRxBufferSize(4096);   // Must precede Serial.begin()
  Serial.begin(115200);

  // ---- GFX init --------------------------------------------
  bus   = new Arduino_ESP32SPI(LCD_PIN_DC, LCD_PIN_CS,
                                LCD_PIN_SCK, LCD_PIN_MOSI);

  panel = new Arduino_ST7789(bus, LCD_PIN_RST,
                              0 /*rotation*/, false /*IPS*/,
                              172, 320,
                              34, 0,
                              34, 0);

  gfx   = new Arduino_Canvas_PSRAM(172, 320, panel, 0, 0, 0);

  if (!gfx->begin()) {
    Serial.println("ERR: GFX begin failed");
    while (true) delay(1000);
  }

  jd9853_reg_init(bus);

  pinMode(LCD_PIN_BL, OUTPUT);
  digitalWrite(LCD_PIN_BL, HIGH);

  gfx->fillScreen(COL_BG);
  gfx->flush();

  // ---- USB composite device init ---------------------------
  mscBegin();
  hidBegin();

  // ---- Touch init ------------------------------------------
  if (!axs5106l_init(TOUCH_PIN_SDA, TOUCH_PIN_SCL,
                     TOUCH_PIN_RST, TOUCH_PIN_INT)) {
    Serial.println("WARN: Touch not found on I2C 0x63");
  }

  // ---- NVS + LittleFS --------------------------------------
  loadNvsProfiles();
  loadMacros();
  loadQuickBtns();
  littleFsBegin();

  // Load display-flip preference (same "rigcfg" namespace)
  { Preferences _p; _p.begin("rigcfg", true);
    displayFlipped = _p.getBool("flip", false); _p.end(); }
  if (displayFlipped) gfx->setRotation(2);

  // ---- Splash + async Wi-Fi scan ---------------------------
  //
  //  Step 1: Kick off async Wi-Fi scan immediately (non-blocking).
  //          The scan runs in the background while logos are shown.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.scanNetworks(true /*async*/, true /*show_hidden*/);

  //  Step 2: White screen + HMS logo (upper half).
  gfx->fillScreen(COL_SPLASH_BG);
  _splashTop();
  gfx->flush();
  delay(1000);   // 1 s with HMS logo visible

  //  Step 3: Add Anybus logo (lower half) — still white bg.
  _splashBottom();
  gfx->flush();

  //  Step 4: Wait for scan to finish (cap 3 s; usually done already).
  {
    unsigned long scanDeadline = millis() + 3000UL;
    while (WiFi.scanComplete() == WIFI_SCAN_RUNNING &&
           millis() < scanDeadline) {
      delay(50);
    }
  }

  //  Step 5: Auto-connect using the completed scan results.
  wifiAutoConnect();   // reuses WiFi.scanComplete() — does NOT re-scan

  //  Step 6: Clear splash (logos have solid black bg — looks bad on menu).
  gfx->fillScreen(COL_BG);

  // ---- Home screen -----------------------------------------
  drawHeader("HMS Rig Console");
  drawStatusBar();         // Reflects ESP SSID/IP if connected
  drawTouchBar("MENU", "SELECT");
  drawHomeContent();
  gfx->flush();

  // ---- Serial handshake ------------------------------------
  delay(200);
  Serial.println("HELLO");
}


// ============================================================
//  LOOP
// ============================================================
void loop() {
  handleTouch();
  handleSerial();

  // Web server: active in AP mode AND/OR when ESP is in STA mode
  if (currentState == STATE_AP_ACTIVE || espConnected) {
    server.handleClient();
    wsServer.loop();
  }

  // Async Wi-Fi scan polling
  if (currentState == STATE_ESP_SCAN) pollWifiScan();
  if (currentState == STATE_PI_SCAN)  pollPiScan();

  // Install-on-Pi requested from web UI (set by handleApiInstallOnPi)
  if (pendingPiInstall) {
    pendingPiInstall = false;
    hidInstallOnPi();
  }

  unsigned long now = millis();

  // Poll Pi every POLL_INTERVAL_MS (after first ACK)
  if (lastStatusMs > 0 && (now - lastPollMs) > POLL_INTERVAL_MS) {
    Serial.println("CMD:GET_STATUS");
    lastPollMs = now;
  }

  // Offline detection
  if (piConnected && lastStatusMs > 0 &&
      (now - lastStatusMs) > PI_TIMEOUT_MS) {
    piConnected = false;
    drawStatusBar();
    gfx->flush();
  }
}
