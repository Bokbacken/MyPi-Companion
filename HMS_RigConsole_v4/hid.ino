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
  HIDKeyboard.begin();
  USB.begin();
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
//  hidType()  — type a string in the terminal and press Enter.
// ----------------------------------------------------------
static void hidType(const char *s) {
  HIDKeyboard.println(s);
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

  // NOTE: setxkbmap us is intentionally NOT used here.
  // Under Wayland, HID keyboard input is processed by the Wayland
  // compositor (Swedish layout) before X11 ever sees it, so
  // setxkbmap has no effect on HID keycodes.
  //
  // Solution: avoid the two characters that differ on Swedish vs US:
  //   |  (pipe)         → arrives as ~  on Swedish layout
  //   "  (double quote) → arrives as @  on Swedish layout
  // All other special chars ($, (, ), /, {, }, ;, >, -) are identical.

  // ── 3. Find RIGSETUP mount point (disk already inserted in step 1) ──
  hidSetStatus(0, "Waiting for disk", COL_DIM);
  while (Serial.available()) Serial.read();   // flush RX

  String mountPath;

  for (int attempt = 0; attempt < 20 && mountPath.isEmpty(); attempt++) {
    // Broadcast find result to all possible ACM ports.
    // Writing to a tmpfile first is not needed — echoing directly
    // to each ACM port is safe: if the port does not exist or we
    // lack permission, 2>/dev/null suppresses the error.
    hidType(
      "for D in $(find /media /run/media -maxdepth 3 -name RIGSETUP -type d 2>/dev/null);"
      " do for P in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyACM2 /dev/ttyACM3;"
      " do echo RIGSETUP:$D > $P 2>/dev/null; done; break; done"
    );

    // Read Serial for up to 1500 ms.
    unsigned long t0 = millis();
    String rxLine;
    while (millis() - t0 < 1500) {
      while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
          rxLine.trim();
          if (rxLine.startsWith("RIGSETUP:")) {
            mountPath = rxLine.substring(9);
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
      snprintf(buf, sizeof(buf), "Waiting... %2ds", 20 - attempt);
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
               + "echo ACMOK:" + String(i) + " > " + port + " 2>/dev/null";
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
          hidSetStatus(0, "Done!",      COL_ACTIVE);
          hidSetStatus(1, "Reboot Pi", COL_ACTIVE);
          mscEject();
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
