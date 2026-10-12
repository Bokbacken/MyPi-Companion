#pragma once
// ============================================================
//  HMS Rig Console — Hardware / Layout Configuration  (v6)
//  Target: Waveshare ESP32-S3-Touch-LCD-1.47
//          JD9853 display  172×320 portrait
//          AXS5106L capacitive touch  (I2C, addr 0x63)
// ============================================================

// -----------------------------------------------------------
//  LCD SPI pins  (JD9853 / Arduino_GFX)
// -----------------------------------------------------------
#define LCD_PIN_DC    45
#define LCD_PIN_CS    21
#define LCD_PIN_SCK   38
#define LCD_PIN_MOSI  39
#define LCD_PIN_RST   40
#define LCD_PIN_BL    46

// -----------------------------------------------------------
//  Touch I2C pins  (AXS5106L)
// -----------------------------------------------------------
#define TOUCH_PIN_SDA  42
#define TOUCH_PIN_SCL  41
#define TOUCH_PIN_RST  47
#define TOUCH_PIN_INT  48
#define TOUCH_I2C_ADDR 0x63

// -----------------------------------------------------------
//  Screen geometry  (portrait 172×320)
//
//   y=  0.. 19   Header bar      (20 px)
//   y= 20..235   Content area   (216 px)  — menus / info
//   y=236..299   Status bar      (64 px)  — 4 info lines
//   y=300..319   Touch bar       (20 px)  — NEXT | SELECT
//
//  Status bar lines:
//    line 1 (y=240): ESP SSID
//    line 2 (y=254): ESP IP address
//    line 3 (y=268): Pi status
//    line 4 (y=282): Pi active SSID / error message
// -----------------------------------------------------------
#define SCREEN_W      172
#define SCREEN_H      320

#define HDR_H          20
#define CONTENT_TOP    20
#define CONTENT_H     216     // (was 230) — reduced to give status 4 lines
#define STATUS_TOP    236     // first pixel of status bar
#define STATUS_H       64     // 4 lines x 14 px + padding
#define TOUCH_TOP     300
#define TOUCH_H        20

// Compact menu row — single-line text + minimal padding
#define MENU_ROW_H     18     // 8 px text + 5 px top + 5 px bottom
#define ADV_MENU_COUNT  6     // Advanced menu: AP, info, install, flip, AP on boot, back

// Status bar text baselines (y offset from STATUS_TOP)
#define SB_LINE1        4     // ESP SSID
#define SB_LINE2       18     // ESP IP
#define SB_LINE3       32     // Pi status
#define SB_LINE4       46     // Pi active SSID / error

// Touch tap zone
#define TOUCH_X_MID   (SCREEN_W / 2)

// -----------------------------------------------------------
//  Colors  (RGB565)
// -----------------------------------------------------------
#define COL_BG        ((uint16_t)0x0000)
#define COL_HEADER    ((uint16_t)0x0215)
#define COL_HDR_BG    ((uint16_t)0xFFFF)
#define COL_HDR_TXT   ((uint16_t)0x19CD)
#define COL_ACCENT    ((uint16_t)0xF3E4)
#define COL_TEXT      ((uint16_t)0xFFFF)
#define COL_DIM       ((uint16_t)0x7BEF)
#define COL_ACTIVE    ((uint16_t)0x07E0)
#define COL_BORDER    ((uint16_t)0x2104)
#define COL_STATUS_BG ((uint16_t)0x0842)
#define COL_ERR       ((uint16_t)0xF800)

// -----------------------------------------------------------
//  AP credentials
// -----------------------------------------------------------
#define AP_SSID_STR  "RigSetup"
#define AP_PASS_STR  "hms12345"

// NVS keys (namespace "rigcfg")
#define NVS_KEY_AP_BOOT   "ap_boot"   // bool: start AP automatically on boot

// -----------------------------------------------------------
//  Serial polling / offline detection
// -----------------------------------------------------------
#define POLL_INTERVAL_MS   5000UL
#define PI_TIMEOUT_MS     15000UL

// -----------------------------------------------------------
//  Touch debounce + gesture thresholds
// -----------------------------------------------------------
#define TOUCH_DEBOUNCE_MS  200UL
#define SWIPE_MIN_PX        22   // min displacement (px) to classify as swipe
#define TAP_MAX_PX          14   // max displacement (px) to classify as tap

// -----------------------------------------------------------
//  Wi-Fi / Profile limits
//
//  NVS slots are SHARED — used both as:
//    * ESP auto-connect profiles  (slot order = priority, DHCP)
//    * Pi profiles (push via CMD:ADDWIFI_FULL over serial)
//
//  Slot 0 has highest priority at boot.
//  Example: slot 0 = PITCSE01 (site), slot 1 = TOFTA (home).
// -----------------------------------------------------------
#define MAX_NVS_SLOTS      10   // shared ESP+Pi (was 4)
#define MAX_WIFI_PROFILES   5
#define MAX_PROFILE_LEN    33
#define MAX_PI_PROFILES    16

// Per-slot connect timeout at boot
#define WIFI_CONNECT_TIMEOUT_MS  8000UL

// -----------------------------------------------------------
//  Wi-Fi scan + keyboard
// -----------------------------------------------------------
#define MAX_SCAN_RESULTS        20
#define KB_RANGE_COUNT           6

// -----------------------------------------------------------
//  Structs
// -----------------------------------------------------------

// Menu list item — used by drawMenuList() in display.ino.
// Must live in config.h so it is visible before Arduino generates
// forward declarations for functions that take MenuItem*.
struct MenuItem {
  const char *label;
  uint16_t    fg;   // normal foreground colour
};

struct NvsProfile {
  char ssid[33];
  char pass[65];
  bool dhcp;
  char ip[16];
  char sn[16];
  char gw[16];
  char dns[16];
  bool valid;
};

#define MAX_AP_PROFILES 4

struct ApProfile {
  char    name[24];   // user label
  char    ssid[33];   // AP broadcast SSID
  char    pass[65];   // WPA2 password
  uint8_t subnet;     // 0-3 (index into AP_SUBNET_DEFS)
  bool    nat;        // enable NAPT internet sharing
  bool    valid;
};

struct PiProfile {
  char ssid[33];
  char pass[65];
};

// -----------------------------------------------------------
//  Touch gesture actions
// -----------------------------------------------------------
enum TouchAction {
  ACT_NONE,
  ACT_TAP_LEFT, ACT_TAP_MID, ACT_TAP_RIGHT,
  ACT_SWIPE_UP, ACT_SWIPE_DOWN, ACT_SWIPE_LEFT, ACT_SWIPE_RIGHT,
};

// -----------------------------------------------------------
//  Terminal macros
// -----------------------------------------------------------
#define MAX_MACROS      24
#define MACRO_NAME_LEN  25    // 24 displayable chars + null
#define MACRO_CMD_LEN  161    // 160 chars + null

struct TermMacro {
  char name[MACRO_NAME_LEN];
  char cmd[MACRO_CMD_LEN];
  bool valid;
};

// -----------------------------------------------------------
//  State machine
// -----------------------------------------------------------
enum State {
  STATE_STATUS,       // home screen
  STATE_MENU,         // top-level menu (4 items)
  STATE_PI_MENU,      // Pi's nmcli profiles — tell Pi to connect
  STATE_NVS_MENU,     // shared NVS slots — push profile to Pi
  STATE_ESP_MENU,     // shared NVS slots — connect ESP
  STATE_ESP_SCAN,     // ESP radio scan results — pick SSID to add (2.4 GHz)
  STATE_PI_SCAN,      // Pi nmcli scan results — pick SSID to add (2.4 + 5 GHz)
  STATE_ESP_KB,       // character-picker — enter password
  STATE_ADVANCED,     // AP / info / Install on Pi
  STATE_ADV_INFO,     // system diagnostics
  STATE_AP_ACTIVE,    // AP mode running
  STATE_AP_SELECT     // AP profile selection menu (pick 0-3 before starting)
};
