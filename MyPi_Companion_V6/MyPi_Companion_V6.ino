// ============================================================
//  MyPi Companion — Firmware v6
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
//    MyPi_Companion_V6.ino — includes, globals, setup, loop, splash
//    display.ino           — all draw*() functions  (Arduino_GFX)
//    touch.ino             — handleTouch, dispatchAction, pollWifiScan
//    protocol.ino          — handleSerial, processMsg, wsEvent
//    nvs_profiles.ino      — loadNvsProfiles, saveNvsSlot, deleteNvsSlot
//    ap_profiles.ino       — loadApProfiles, saveApProfile, apComputeNetwork
//    web_server.ino        — HTTP handlers, startWebServer, startAP/stopAP
//    wifi_manager.ino      — wifiAutoConnect, wifiConnectSlot, espSSID/IP
//
//  Arduino IDE settings:
//    Board       : ESP32S3 Dev Module
//    USB Mode    : USB-OTG (TinyUSB)   ← REQUIRED (not "Hardware CDC")
//    USB CDC     : Enabled
//    PSRAM       : OPI PSRAM
//    Flash size  : 16 MB  (or 8 MB)
//    Upload speed: 921600
//
//  Note: "USB-OTG (TinyUSB)" maps to USBMode=default in the FQBN.
//  This gives a composite CDC serial + HID keyboard + MSC device.
//  With "Hardware CDC and JTAG" (USBMode=hwcdc), HID and MSC do not
//  work and the serial port only appears in bootloader mode.
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
#include "logo_bitmaps.h"          // MyPi Companion splash + header icon

// -----------------------------------------------------------
//  GFX objects  (heap-allocated; gfx is the canvas)
// -----------------------------------------------------------
Arduino_DataBus *bus   = nullptr;
Arduino_GFX    *panel  = nullptr;
Arduino_Canvas_PSRAM *gfx    = nullptr;

// -----------------------------------------------------------
//  AP objects
// -----------------------------------------------------------
// AP profiles (loaded from NVS on boot — ap_profiles.ino)
ApProfile   apProfiles[MAX_AP_PROFILES];
int         activeApProfile = 0;    // last started AP slot
bool        apOnBoot        = false; // NVS: start AP automatically on boot
int         apSelectIndex   = 0;    // cursor in STATE_AP_SELECT menu
// Runtime AP network settings (populated by startAP(slot))
IPAddress   dynApIp, dynApGw, dynApSn;

// Subnet definitions [subnet_idx] = {a, b, c}  -> IP a.b.c.1
static const uint8_t AP_SUBNET_DEFS[MAX_AP_PROFILES][3] = {
  {192, 168,  4},
  {192, 168, 42},
  { 10,  11,  0},
  {172,  16,  4},
};
static const char* AP_SUBNET_LABELS[MAX_AP_PROFILES] = {
  "192.168.4.x", "192.168.42.x", "10.11.0.x", "172.16.4.x"
};

// -----------------------------------------------------------
//  Web server / WebSocket objects
// -----------------------------------------------------------
WebServer        server(80);
WebSocketsServer wsServer(81);
#define MAX_TERM_SESSIONS 4
int8_t termSlotClient[MAX_TERM_SESSIONS] = {-1, -1, -1, -1};
// Maps session slot (0-3) -> WebSocket client num, -1 = free

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
char  infoMsg[64]                 = "";     // transient status (status bar line 3)
bool  hasInfo                     = false;
unsigned long infoExpireMs        = 0;      // millis() when infoMsg is cleared

// Pi active network interfaces (received via STATUS:IFACES:)
PiIface piIfaces[MAX_PI_IFACES];
int     piIfaceCount   = 0;
int     piIfaceIdx     = 0;   // currently displayed interface
unsigned long lastIfaceScrollMs = 0;

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
volatile int  pendingApStart   = -1;   // AP profile slot to start from web (-1 = none)

// Install target — see hid.ino.
//   piInstallMode : 0 = Desktop (Ctrl+Alt+T + automount), 1 = CLI (console login)
//   piUser        : Pi login name (persisted in NVS "rigcfg"/"instUser")
//   piPass        : Pi password — RAM ONLY, wiped after every install attempt
//   piLayoutForce : -1 = auto-detect keyboard layout, else HID_LAYOUTS index
uint8_t piInstallMode = 0;
char    piUser[33]    = "";
char    piPass[65]    = "";
int8_t  piLayoutForce = -1;

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

// KB mode — controls what kbOk() does after text entry
enum KbMode { KB_MODE_WIFI, KB_MODE_INSTALL_USER, KB_MODE_INSTALL_PASS };
KbMode kbMode = KB_MODE_WIFI;

// Advanced menu: GUI install confirmation pending
bool advGuiConfirmPending = false;

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
//  Full-screen MyPi Companion logo (172×320, black background).
//
//  Sequence:
//    1. Start async Wi-Fi scan (non-blocking) immediately.
//    2. Draw splash → flush → delay 1 s.
//    3. Poll until scan done (cap 3 s) → menu appears.
//
//  This overlaps the scan time with the splash display,
//  cutting perceived startup time significantly.
// ============================================================

static void _drawSplash() {
  drawProgmemImage(0, 0, LOGO_SPLASH_W, LOGO_SPLASH_H, logo_splash);
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
  loadApProfiles();
  loadMacros();
  loadQuickBtns();
  littleFsBegin();

  // Load display-flip preference (same "rigcfg" namespace)
  { Preferences _p; _p.begin("rigcfg", true);
    displayFlipped = _p.getBool("flip", false);
    apOnBoot       = _p.getBool(NVS_KEY_AP_BOOT, false);
    // Install target (password is never stored)
    piInstallMode = _p.getUChar("instMode", 0);
    piLayoutForce = _p.getChar("instLayout", -1);
    _p.getString("instUser", piUser, sizeof(piUser));
    _p.end(); }
  if (displayFlipped) gfx->setRotation(2);

  // ---- Splash + async Wi-Fi scan ---------------------------
  //
  //  Step 1: Kick off async Wi-Fi scan immediately (non-blocking).
  //          The scan runs in the background while logos are shown.
  wifiDiagInit();          // count STA drops for /api/apstatus
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.scanNetworks(true /*async*/, true /*show_hidden*/);

  //  Step 2: Full-screen MyPi Companion splash.
  _drawSplash();
  gfx->flush();
  delay(1000);   // 1 s with splash visible

  //  Step 3: Wait for scan to finish (cap 3 s; usually done already).
  {
    unsigned long scanDeadline = millis() + 3000UL;
    while (WiFi.scanComplete() == WIFI_SCAN_RUNNING &&
           millis() < scanDeadline) {
      delay(50);
    }
  }

  //  Step 4: Auto-connect using the completed scan results.
  wifiAutoConnect();   // reuses WiFi.scanComplete() — does NOT re-scan

  //  Step 5: Clear splash (logos have solid black bg — looks bad on menu).
  gfx->fillScreen(COL_BG);

  // ---- Home screen -----------------------------------------
  drawHeader("MyPi Companion");
  drawStatusBar();         // Reflects ESP SSID/IP if connected
  drawTouchBar("MENU", "SELECT");
  drawHomeContent();
  gfx->flush();

  // ---- AP on boot -----------------------------------------
  //  Nothing else starts the AP during setup(). Switch to the AP
  //  screen so loop() services the web server even without STA.
  if (apOnBoot && currentState != STATE_AP_ACTIVE) {
    startAP(activeApProfile);
    currentState = STATE_AP_ACTIVE;
    updateContentArea();
  }

  // ---- Serial handshake ------------------------------------
  delay(200);
  Serial.println("HELLO");
}


// ============================================================
//  LOOP
// ============================================================
// Longest loop() pass, in two 10 s windows (current + previous), so
// /api/apstatus can show whether our own code is stalling. NAT itself
// runs in the lwIP task on core 0 and does not depend on loop().
uint32_t loopMaxUs     = 0;
uint32_t loopMaxPrevUs = 0;

static void _loopBody();

void loop() {
  static uint32_t windowStart = 0;
  uint32_t t0 = micros();
  _loopBody();
  uint32_t dt = micros() - t0;
  if (dt > loopMaxUs) loopMaxUs = dt;
  if (millis() - windowStart >= 10000UL) {
    windowStart   = millis();
    loopMaxPrevUs = loopMaxUs;
    loopMaxUs     = 0;
  }
}

static void _loopBody() {
  handleTouch();
  handleSerial();
  mscLoop();   // host-initiated MSC eject (SCSI START STOP UNIT)

  // Web server: active in AP mode AND/OR when ESP is in STA mode
  if (currentState == STATE_AP_ACTIVE || espConnected) {
    server.handleClient();
    wsServer.loop();
  }

  // Async Wi-Fi scan polling
  if (currentState == STATE_ESP_SCAN) pollWifiScan();
  if (currentState == STATE_PI_SCAN)  pollPiScan();
  pollApClients();

  // Install-on-Pi requested from web UI (set by handleApiInstallOnPi)
  if (pendingPiInstall) {
    pendingPiInstall = false;
    hidInstallOnPi();
  }

  // AP start requested from web UI (set by handleApiApStart)
  if (pendingApStart >= 0) {
    int slot = pendingApStart;
    pendingApStart = -1;
    startAP(slot);
    currentState = STATE_AP_ACTIVE;
    updateContentArea();
  }

  // Keep NAT in sync with the STA uplink while the AP is running
  apNatService();

  unsigned long now = millis();

  // Handshake retry. The HELLO from setup() is lost when the daemon
  // is not running yet (Pi still booting): with nobody holding the
  // ACM port open, USBCDC drops the write. (bool)Serial is true only
  // while the host has the port open (DTR), so nothing is sent until
  // the daemon opens it. Repeat because the daemon waits ~1.5 s and
  // flushes its input buffer right after opening.
  static unsigned long lastHelloMs = 0;
  if (!piConnected && Serial && (now - lastHelloMs) >= HELLO_RETRY_MS) {
    Serial.println("HELLO");
    lastHelloMs = now;
  }

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

  // Expire transient info message (e.g. "Tillagd: <ssid>") → back to Pi IP
  if (hasInfo && (long)(now - infoExpireMs) >= 0) {
    hasInfo = false;
    drawStatusBar();
    gfx->flush();
  }

  // Scroll through Pi interfaces every IFACE_SCROLL_MS
  if (piIfaceCount > 1 && (now - lastIfaceScrollMs) >= IFACE_SCROLL_MS) {
    lastIfaceScrollMs = now;
    piIfaceIdx = (piIfaceIdx + 1) % piIfaceCount;
    if (currentState == STATE_STATUS) { drawStatusBar(); gfx->flush(); }
  }
}
