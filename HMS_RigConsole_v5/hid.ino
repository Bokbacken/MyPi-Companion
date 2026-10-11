// ============================================================
//  hid.ino — USB HID keyboard actions
//
//  The ESP32-S3 presents itself to the Pi as a composite USB
//  device (three interfaces on one physical cable):
//    Interface 0 : CDC serial   /dev/ttyACM0   (rig_daemon.py)
//    Interface 1 : HID keyboard /dev/input/evN (no Pi driver needed)
//    Interface 2 : Mass storage /media/<user>/RIGSETUP/
//
//  Board setting required in Arduino IDE:
//    Tools → USB Mode : USB-OTG / TinyUSB
//
//  Key functions:
//    hidBegin()           — call in setup() AFTER mscBegin()
//    hidInstallOnPi()     — full installation flow (called from touch.ino)
//    hidTestOpenTerminal()— quick HID smoke-test (open terminal + ls)
// ============================================================

#include "USB.h"
#include "USBHIDKeyboard.h"

USBHIDKeyboard HIDKeyboard;

// ----------------------------------------------------------
//  hidBegin()  — call once in setup(), AFTER mscBegin()
// ----------------------------------------------------------
void hidBegin() {
  // Default (en_US) map is only used for layout-neutral shortcuts such as
  // Ctrl+Alt+T. Text is typed with hidTypeLayout(), see below.
  HIDKeyboard.begin();
  USB.begin();
}

// ----------------------------------------------------------
//  Keyboard layout handling
//
//  USB HID keyboards cannot send characters — only key positions
//  (usage codes). The Pi translates them with ITS keyboard layout,
//  so e.g. "/" typed with US codes arrives as "-" on a Swedish Pi.
//
//  We therefore auto-detect the Pi's layout: the RIGSETUP probe
//  command is typed with each candidate layout in turn, and only
//  the correctly interpreted one answers over the CDC serial port.
//  A wrongly interpreted probe is a harmless bash syntax error
//  (verified for all pairs below). The detected layout is then used
//  for the rest of the install and tried first next time.
//
//  Layouts that type the probe identically are covered implicitly:
//  sv_SE also covers da_DK; es_ES also covers it_IT and pt_PT.
// ----------------------------------------------------------
static const uint8_t *const HID_LAYOUTS[] = {
  KeyboardLayout_en_US, KeyboardLayout_sv_SE, KeyboardLayout_de_DE,
  KeyboardLayout_fr_FR, KeyboardLayout_es_ES
};
static const char *const HID_LAYOUT_NAMES[] = {
  "en_US", "sv_SE", "de_DE", "fr_FR", "es_ES"
};
static const uint8_t HID_LAYOUT_COUNT = sizeof(HID_LAYOUTS) / sizeof(HID_LAYOUTS[0]);
static uint8_t hidLayoutIdx = 0;   // last detected layout (RAM only)

// Press and release one key with the given modifier bits.
static void hidSendKey(uint8_t mods, uint8_t code) {
  KeyReport r = { mods, 0, { code, 0, 0, 0, 0, 0 } };
  HIDKeyboard.sendReport(&r);
  KeyReport up = { 0, 0, { 0, 0, 0, 0, 0, 0 } };
  HIDKeyboard.sendReport(&up);
}

// Type an ASCII string using the given layout table, then press Enter.
// Decoding of the layout table mirrors USBHIDKeyboard::press().
static void hidTypeLayout(uint8_t idx, const char *s) {
  const uint8_t *map = HID_LAYOUTS[idx];
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    if (c >= 128) continue;
    uint8_t k = map[c];
    if (!k) continue;                       // not typeable in this layout
    uint8_t mods = 0;
    if (k & 0x80) { mods |= 0x02; k &= 0x7F; }   // SHIFT  -> left shift
    if (k & 0x40) { mods |= 0x40; k &= ~0x40; }  // ALT_GR -> right alt
    if (k == 0x32) k = 0x64;                     // ISO_REPLACEMENT -> ISO_KEY
    hidSendKey(mods, k);
  }
  hidSendKey(0, 0x28);                      // Enter
}

// Ctrl+C — clears any half-typed line or continuation prompt (">")
// left behind by a probe typed with the wrong layout.
static void hidCancelLine() {
  hidSendKey(0x01, 0x06);                   // left Ctrl + 'c' key
  delay(150);
}

// ----------------------------------------------------------
//  hidSetStatus()  — update a status line on the display.
//  line 0 = main status, line 1 = detail / path / countdown.
// ----------------------------------------------------------
static void hidSetStatus(uint8_t line, const char *msg, uint16_t col) {
  int16_t y = (line == 0) ? CONTENT_TOP + 20 : CONTENT_TOP + 40;
  char buf[24];
  snprintf(buf, sizeof(buf), "%-20s", msg);
  drawTxt(buf, 4, y, col, COL_BG);
  gfx->flush();
}

// ----------------------------------------------------------
//  hidType()  — type a string with the detected layout + Enter.
// ----------------------------------------------------------
static void hidType(const char *s) {
  hidTypeLayout(hidLayoutIdx, s);
}

// ----------------------------------------------------------
//  hidInstallOnPi()
//
//  Full install flow:
//  1. Open terminal (Ctrl+Alt+T).
//  2. Poll until RIGSETUP volume auto-mounts.
//     Searches both /media and /run/media (newer Pi OS/Ubuntu).
//     Response is echoed back over /dev/ttyACMx so we can read
//     it on the ESP32's Serial RX.
//  3. Probe /dev/ttyACM{0..3} to confirm which port is ours.
//  4. Run:  sudo bash <mount>/install.sh <port>
//  5. Read RIG:* status codes and update display.
//
//  Blocks the loop for up to ~60 s (one-shot admin action).
// ----------------------------------------------------------
void hidInstallOnPi() {

  // ── 1. Insert disk FIRST (before opening terminal) ────────
  //  When the MSC volume appears, the Pi file manager pops a dialog
  //  that steals keyboard focus from whatever window is active.
  //  By inserting the disk before the terminal is opened the dialog
  //  appears (and can be ignored/dismissed), and the terminal that
  //  opens next keeps focus for the rest of the install sequence.
  hidSetStatus(0, "Mounting disk", COL_DIM);
  hidSetStatus(1, "",              COL_DIM);
  mscInsert();   // triggers Pi file-manager dialog
  delay(5000);   // wait for dialog to appear; ignore/dismiss it now

  // ── 2. Open terminal (gets focus — dialog already up/gone) ──
  hidSetStatus(0, "Opening terminal", COL_DIM);

  HIDKeyboard.press(KEY_LEFT_CTRL);
  HIDKeyboard.press(KEY_LEFT_ALT);
  HIDKeyboard.press('t');
  HIDKeyboard.releaseAll();
  delay(3000);   // wait for terminal + shell prompt

  // NOTE: setxkbmap is intentionally NOT used (no effect under Wayland).
  // The Pi's keyboard layout is auto-detected instead — see
  // "Keyboard layout handling" above. Avoid | and " in typed commands.

  // ── 3. Find RIGSETUP mount point + detect keyboard layout ──
  hidSetStatus(0, "Waiting for disk", COL_DIM);
  while (Serial.available()) Serial.read();   // flush RX

  String mountPath;
  const uint8_t firstLayout = hidLayoutIdx;

  for (int attempt = 0; attempt < 20 && mountPath.isEmpty(); attempt++) {
    // Cycle through candidate layouts, starting with the last detected.
    uint8_t idx = (firstLayout + attempt) % HID_LAYOUT_COUNT;

    // Broadcast find result to all possible ACM ports.
    // Echoing directly to each ACM port is safe: if the port does not
    // exist or we lack permission, 2>/dev/null suppresses the error.
    // NOTE: bash applies redirections left to right, so 2>/dev/null
    // must come BEFORE "> $P" or the open error still reaches the tty.
    // The ".:" in the reply also proves "." is typed correctly
    // (needed later for install.sh).
    hidCancelLine();
    hidTypeLayout(idx,
      "for D in $(find /media /run/media -maxdepth 3 -name RIGSETUP -type d 2>/dev/null);"
      " do for P in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyACM2 /dev/ttyACM3;"
      " do echo RIGSETUP:.:$D 2>/dev/null > $P; done; break; done"
    );

    // Read Serial for up to 1500 ms.
    unsigned long t0 = millis();
    String rxLine;
    while (millis() - t0 < 1500) {
      while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
          rxLine.trim();
          if (rxLine.startsWith("RIGSETUP:.:")) {
            mountPath    = rxLine.substring(11);
            hidLayoutIdx = idx;              // this layout works
          }
          rxLine = "";
        } else if (c != '\r') {
          rxLine += c;
        }
      }
      if (!mountPath.isEmpty()) break;
      delay(20);
    }

    if (mountPath.isEmpty()) {
      char buf[20];
      snprintf(buf, sizeof(buf), "Try %s %2d", HID_LAYOUT_NAMES[idx], 20 - attempt);
      hidSetStatus(1, buf, COL_DIM);
      delay(500);
    }
  }

  if (mountPath.isEmpty()) {
    hidSetStatus(0, "ERR: no disk", COL_ERR);
    hidSetStatus(1, "not mounted", COL_ERR);
    mscEject();
    return;
  }

  char mpBuf[24];
  snprintf(mpBuf, sizeof(mpBuf), "%.20s", mountPath.c_str());
  hidSetStatus(1, mpBuf, COL_DIM);

  // ── 3. Find our ACM port ──────────────────────────────────
  hidSetStatus(0, "Finding port...", COL_DIM);

  String acmPort;
  for (int i = 0; i <= 3 && acmPort.isEmpty(); i++) {
    String port = "/dev/ttyACM" + String(i);
    while (Serial.available()) Serial.read();  // flush

    String cmd = "stty -F " + port + " -hupcl 2>/dev/null; "
               + "echo ACMOK:" + String(i) + " 2>/dev/null > " + port;
    hidType(cmd.c_str());

    unsigned long t0 = millis();
    String rxLine;
    bool found = false;
    while (millis() - t0 < 1200) {
      while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
          rxLine.trim();
          if (rxLine.startsWith("ACMOK:")) found = true;
          rxLine = "";
        } else if (c != '\r') {
          rxLine += c;
        }
      }
      if (found) break;
      delay(20);
    }
    if (found) acmPort = port;
  }

  if (acmPort.isEmpty()) {
    hidSetStatus(0, "ERR: no port", COL_ERR);
    hidSetStatus(1, "ACM0-3 failed", COL_ERR);
    mscEject();
    return;
  }

  char portBuf[24];
  snprintf(portBuf, sizeof(portBuf), "Port: %.14s", acmPort.c_str());
  hidSetStatus(1, portBuf, COL_DIM);

  // ── 4. Run installer ──────────────────────────────────────
  hidSetStatus(0, "Running install", COL_DIM);

  String installCmd = "sudo bash " + mountPath + "/install.sh " + acmPort;
  hidType(installCmd.c_str());

  // ── 5. Read RIG:* feedback from install.sh ─────────────────
  String rxLine;
  unsigned long deadline = millis() + 60000UL;

  while (millis() < deadline) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        rxLine.trim();
        if      (rxLine == "RIG:INSTALL_START")   hidSetStatus(0, "Starting...",    COL_DIM);
        else if (rxLine == "RIG:INSTALL_DEPS")    hidSetStatus(0, "Install deps",   COL_DIM);
        else if (rxLine == "RIG:INSTALL_DAEMON")  hidSetStatus(0, "Copying daemon", COL_DIM);
        else if (rxLine == "RIG:INSTALL_SERVICE") hidSetStatus(0, "Setup service",  COL_DIM);
        else if (rxLine == "RIG:INSTALL_DONE") {
          hidSetStatus(0, "Done!",         COL_ACTIVE);
          hidSetStatus(1, "Connecting...", COL_DIM);
          // Handshake BEFORE ejecting: with piConnected set, mscEject()
          // asks the daemon to sync + unmount RIGSETUP (CMD:MSC_EJECT)
          // instead of pulling the media while the Pi still has it
          // mounted ("unable to read inode block" in dmesg).
          // Give the newly started daemon time to open the ACM port.
          delay(3000);
          while (Serial.available()) Serial.read();  // flush stale bytes
          Serial.println("HELLO");
          uint32_t tHello = millis();
          while (!piConnected && millis() - tHello < 5000) {
            handleSerial();                          // picks up ACK
            delay(20);
          }
          mscEject();   // graceful if connected, plain removal otherwise
          return;
        }
        else if (rxLine.startsWith("RIG:INSTALL_ERR")) {
          char errBuf[24];
          snprintf(errBuf, sizeof(errBuf), "%.20s", rxLine.c_str() + 4);
          hidSetStatus(0, "ERR: install", COL_ERR);
          hidSetStatus(1, errBuf,         COL_ERR);
          mscEject();
          return;
        }
        rxLine = "";
      } else if (c != '\r') {
        rxLine += c;
      }
    }
    delay(50);
  }

  mscEject();
  hidSetStatus(0, "Timeout!", COL_ERR);
  hidSetStatus(1, "Check Pi screen", COL_ERR);
}

// ----------------------------------------------------------
//  hidTestOpenTerminal()
//  Quick smoke-test: Ctrl+Alt+T + "ls" + Enter.
// ----------------------------------------------------------
void hidTestOpenTerminal() {
  HIDKeyboard.press(KEY_LEFT_CTRL);
  HIDKeyboard.press(KEY_LEFT_ALT);
  HIDKeyboard.press('t');
  HIDKeyboard.releaseAll();
  delay(1200);

  HIDKeyboard.print("ls");
  delay(80);
  HIDKeyboard.press(KEY_RETURN);
  HIDKeyboard.releaseAll();
}
