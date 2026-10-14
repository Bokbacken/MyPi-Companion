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
//    hidInstallOnPi()     — full installation flow, Desktop or CLI mode
//                           (called from touch.ino and via the web page)
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

// Install progress — written by hidSetStatus(), read via /api/installstatus
char     hidStatusMsg[2][24]  = {"", ""};
bool     hidStatusErr[2]      = {false, false};
volatile bool hidInstallActive = false;

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
  if (line < 2) {
    strlcpy(hidStatusMsg[line], msg, sizeof(hidStatusMsg[0]));
    hidStatusErr[line] = (col == COL_ERR);
  }
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
//  Serial line helpers
// ----------------------------------------------------------
static void hidFlushRx() {
  while (Serial.available()) Serial.read();
}

// Wait up to ms for a line starting with prefix on the CDC port.
// Other lines (e.g. daemon traffic during a reinstall) are ignored.
static bool hidWaitLine(const char *prefix, uint32_t ms, String *line = nullptr) {
  unsigned long t0 = millis();
  String rx;
  while (millis() - t0 < ms) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        rx.trim();
        if (rx.startsWith(prefix)) { if (line) *line = rx; return true; }
        rx = "";
      } else if (c != '\r') {
        rx += c;
      }
    }
    delay(20);
  }
  return false;
}

// Wipe the RAM-only Pi password (called at every exit of hidInstallOnPi).
static void hidWipePass() {
  memset(piPass, 0, sizeof(piPass));
}

static bool hidLayoutForced() {
  return piLayoutForce >= 0 && piLayoutForce < (int8_t)HID_LAYOUT_COUNT;
}

// ----------------------------------------------------------
//  Desktop preparation (unchanged v5 flow)
//  Insert disk, Ctrl+Alt+T, find the automounted RIGSETUP
//  directory and detect the keyboard layout on the way.
// ----------------------------------------------------------
static bool hidPrepDesktop(String &mountPath) {

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
  hidFlushRx();

  const uint8_t firstLayout = hidLayoutIdx;

  for (int attempt = 0; attempt < 20 && mountPath.isEmpty(); attempt++) {
    // Cycle through candidate layouts, starting with the last detected
    // (or stay on the layout chosen on the web page).
    uint8_t idx = hidLayoutForced() ? (uint8_t)piLayoutForce
                                    : (firstLayout + attempt) % HID_LAYOUT_COUNT;

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

    String line;
    if (hidWaitLine("RIGSETUP:.:", 1500, &line)) {
      mountPath    = line.substring(11);
      hidLayoutIdx = idx;                    // this layout works
    } else {
      char buf[20];
      snprintf(buf, sizeof(buf), "Try %s %2d", HID_LAYOUT_NAMES[idx], 20 - attempt);
      hidSetStatus(1, buf, COL_DIM);
      delay(500);
    }
  }

  if (mountPath.isEmpty()) {
    hidSetStatus(0, "ERR: no disk", COL_ERR);
    hidSetStatus(1, "not mounted", COL_ERR);
    return false;
  }
  return true;
}

// ----------------------------------------------------------
//  CLI (headless) preparation
//
//  Target: Raspberry Pi OS Lite on the console (tty1) — no
//  desktop, no automount. The HID keyboard types straight into
//  the console, so we can log in like a person would:
//
//   a. Already at a shell (console autologin, open session)?
//      A ping command is typed with every candidate layout; at a
//      shell the right one answers over CDC, at a login prompt they
//      just become failed login attempts. The password is therefore
//      NEVER typed into a shell where it could end up in history.
//   b. Otherwise log in: reset the prompt (Ctrl+C, Enter, wait out
//      the "Login incorrect" delay), type user + password, then ping.
//      Repeated per layout until the ping answers.
//   c. Find our ACM port (shared with desktop flow).
//   d. Authenticate sudo: the password is read with "read -rs"
//      (not echoed, not in history) and fed to "sudo -S -v".
//      Works with and without NOPASSWD sudo.
//   e. Mount RIGSETUP by label at /mnt/rigsetup (read-only).
// ----------------------------------------------------------
static const char HID_CLI_MNT[] = "/mnt/rigsetup";
static const char HID_PING[] =
  "for P in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyACM2 /dev/ttyACM3;"
  " do echo RIGPING:.:$P 2>/dev/null > $P; done";

static void hidEnter() { hidSendKey(0, 0x28); }

// Bring the console back to a fresh "login:" prompt (or a clean shell line).
// The 4 s wait covers login's FAIL_DELAY after an aborted password prompt.
static void hidConsoleReset() {
  hidCancelLine();
  hidEnter();
  delay(4000);
}

static bool hidCliLogin(bool &weLoggedIn) {
  weLoggedIn = false;
  const bool forced = hidLayoutForced();
  const uint8_t n   = forced ? 1 : HID_LAYOUT_COUNT;
  const uint8_t first = forced ? (uint8_t)piLayoutForce : hidLayoutIdx;
  char buf[24];

  // ── a. Already at a shell? ─────────────────────────────────
  hidSetStatus(0, "Checking console", COL_DIM);
  hidCancelLine();
  hidEnter();
  delay(800);
  for (uint8_t i = 0; i < n; i++) {
    uint8_t idx = (first + i) % HID_LAYOUT_COUNT;
    snprintf(buf, sizeof(buf), "Shell? %s", HID_LAYOUT_NAMES[idx]);
    hidSetStatus(1, buf, COL_DIM);
    hidFlushRx();
    hidTypeLayout(idx, HID_PING);
    if (hidWaitLine("RIGPING:.:", 1500)) {
      hidLayoutIdx = idx;
      hidSetStatus(1, "Shell already open", COL_DIM);
      return true;
    }
  }

  // ── b. Log in ──────────────────────────────────────────────
  hidSetStatus(0, "Logging in", COL_DIM);
  for (uint8_t i = 0; i < n; i++) {
    uint8_t idx = (first + i) % HID_LAYOUT_COUNT;
    snprintf(buf, sizeof(buf), "Login %s", HID_LAYOUT_NAMES[idx]);
    hidSetStatus(1, buf, COL_DIM);

    hidConsoleReset();
    hidTypeLayout(idx, piUser);
    delay(1500);                    // "Password:" prompt
    hidTypeLayout(idx, piPass);
    delay(5000);                    // PAM + motd + shell start

    hidFlushRx();
    hidTypeLayout(idx, HID_PING);
    if (hidWaitLine("RIGPING:.:", 2500)) {
      hidLayoutIdx = idx;
      weLoggedIn   = true;
      return true;
    }
  }

  hidSetStatus(0, "ERR: login", COL_ERR);
  hidSetStatus(1, "user/pass/layout?", COL_ERR);
  return false;
}

static bool hidCliSudo(const String &port) {
  hidSetStatus(0, "sudo auth", COL_DIM);
  hidFlushRx();
  hidType("read -rs RIGPW");
  delay(400);
  hidType(piPass);                  // consumed by read, never echoed
  delay(400);
  String cmd = "sudo -S -v <<<$RIGPW 2>/dev/null; unset RIGPW;"
               " sudo -n true 2>/dev/null && echo SUDOOK 2>/dev/null > " + port;
  hidType(cmd.c_str());
  if (hidWaitLine("SUDOOK", 8000)) return true;
  hidSetStatus(0, "ERR: sudo", COL_ERR);
  hidSetStatus(1, "wrong password?", COL_ERR);
  return false;
}

static bool hidCliMount(const String &port) {
  hidSetStatus(0, "Mounting disk", COL_DIM);
  String pre = String("sudo umount -l ") + HID_CLI_MNT + " 2>/dev/null; sudo mkdir -p " + HID_CLI_MNT;
  hidType(pre.c_str());
  delay(500);

  String cmd = String("sudo mount -o ro /dev/disk/by-label/RIGSETUP ") + HID_CLI_MNT +
               " 2>/dev/null; mountpoint -q " + HID_CLI_MNT +
               " && echo MNTOK 2>/dev/null > " + port;
  for (int attempt = 0; attempt < 10; attempt++) {
    char buf[20];
    snprintf(buf, sizeof(buf), "Try %2d", 10 - attempt);
    hidSetStatus(1, buf, COL_DIM);
    hidFlushRx();
    hidType(cmd.c_str());
    if (hidWaitLine("MNTOK", 1500)) return true;
    delay(500);
  }
  hidSetStatus(0, "ERR: no disk", COL_ERR);
  hidSetStatus(1, "mount failed", COL_ERR);
  return false;
}

// Undo what the CLI flow did on the console, as ONE command line.
// Type-ahead is NOT safe here: Debian's sudoers sets "use_pty", so
// while a sudo command runs, sudo relays the console input into the
// command's own pty, where it is echoed and then discarded. A
// separately typed "exit" got lost that way and left tty1 logged in.
// Chaining with ";" means nothing is typed while sudo is running.
// The short delay lets install.sh (itself run via sudo) finish
// printing its banner and exit after RIG:INSTALL_DONE.
static void hidCliCleanup(bool mounted, bool weLoggedIn) {
  delay(1500);
  String cmd;
  if (mounted) cmd = String("cd /; sudo umount ") + HID_CLI_MNT + " 2>/dev/null; ";
  cmd += "sudo -k";                       // drop the cached sudo credentials
  if (weLoggedIn) cmd += "; exit";        // leave the console at the login prompt
  hidType(cmd.c_str());
}

// ----------------------------------------------------------
//  hidFindPort() — probe /dev/ttyACM{0..3} to confirm which
//  Pi-side port is ours (shared by both flows).
// ----------------------------------------------------------
static bool hidFindPort(String &acmPort) {
  hidSetStatus(0, "Finding port...", COL_DIM);

  for (int i = 0; i <= 3 && acmPort.isEmpty(); i++) {
    String port = "/dev/ttyACM" + String(i);
    hidFlushRx();

    String cmd = "stty -F " + port + " -hupcl 2>/dev/null; "
               + "echo ACMOK:" + String(i) + " 2>/dev/null > " + port;
    hidType(cmd.c_str());

    if (hidWaitLine("ACMOK:", 1200)) acmPort = port;
  }

  if (acmPort.isEmpty()) {
    hidSetStatus(0, "ERR: no port", COL_ERR);
    hidSetStatus(1, "ACM0-3 failed", COL_ERR);
    return false;
  }

  char portBuf[24];
  snprintf(portBuf, sizeof(portBuf), "Port: %.14s", acmPort.c_str());
  hidSetStatus(1, portBuf, COL_DIM);
  return true;
}

// ----------------------------------------------------------
//  hidInstallOnPi()
//
//  Desktop mode (piInstallMode == 0):
//    disk → Ctrl+Alt+T → find automount → port → install
//  CLI mode (piInstallMode == 1):
//    disk → console login → port → sudo → mount by label → install
//    → unmount + logout
//  Both: read RIG:* status codes from install.sh, then HELLO/ACK
//  handshake and graceful eject.
//
//  Blocks the loop for up to ~2 min (one-shot admin action).
// ----------------------------------------------------------
void hidInstallOnPi() {
  const bool cli = (piInstallMode == 1);
  String mountPath, acmPort;
  bool weLoggedIn = false, mounted = false;

  hidStatusMsg[0][0] = 0;
  hidStatusMsg[1][0] = 0;
  hidStatusErr[0] = hidStatusErr[1] = false;
  hidInstallActive = true;

  if (cli && (!piUser[0] || !piPass[0])) {
    // The password only lives in RAM and is wiped after each attempt,
    // so CLI installs must be started from the web page.
    hidSetStatus(0, "ERR: no login", COL_ERR);
    hidSetStatus(1, "Start from web", COL_ERR);
    hidWipePass();
    hidInstallActive = false;
    return;
  }

  // ── Preparation ───────────────────────────────────────────
  bool ok;
  if (cli) {
    hidSetStatus(0, "CLI install", COL_DIM);
    hidSetStatus(1, "",            COL_DIM);
    mscInsert();          // no automount on Lite — mounted by label later
    delay(2000);
    ok = hidCliLogin(weLoggedIn) && hidFindPort(acmPort) && hidCliSudo(acmPort);
    if (ok) {
      ok = hidCliMount(acmPort);
      mounted = ok;
      mountPath = HID_CLI_MNT;
    }
  } else {
    ok = hidPrepDesktop(mountPath);
    if (ok) {
      char mpBuf[24];
      snprintf(mpBuf, sizeof(mpBuf), "%.20s", mountPath.c_str());
      hidSetStatus(1, mpBuf, COL_DIM);
      ok = hidFindPort(acmPort);
    }
  }

  if (!ok) {
    if (cli) hidCliCleanup(mounted, weLoggedIn);
    hidWipePass();
    mscEject();
    hidInstallActive = false;
    return;
  }

  // ── Run installer ─────────────────────────────────────────
  hidSetStatus(0, "Running install", COL_DIM);

  String installCmd = "sudo bash " + mountPath + "/install.sh " + acmPort;
  hidType(installCmd.c_str());

  // ── Read RIG:* feedback from install.sh ───────────────────
  String rxLine;
  unsigned long deadline = millis() + 90000UL;   // offline pip may be slow on Lite

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
          toastInfo("Install DONE");
          // CLI: unmount our manual mount and log out (typed ahead).
          if (cli) hidCliCleanup(mounted, weLoggedIn);
          hidWipePass();
          // Handshake BEFORE ejecting: with piConnected set, mscEject()
          // asks the daemon to sync + unmount RIGSETUP (CMD:MSC_EJECT)
          // instead of pulling the media while the Pi still has it
          // mounted ("unable to read inode block" in dmesg).
          // Give the newly started daemon time to open the ACM port.
          delay(3000);
          hidFlushRx();                              // flush stale bytes
          Serial.println("HELLO");
          uint32_t tHello = millis();
          while (!piConnected && millis() - tHello < 5000) {
            handleSerial();                          // picks up ACK
            delay(20);
          }
          mscEject();   // graceful if connected, plain removal otherwise
          hidInstallActive = false;
          return;
        }
        else if (rxLine.startsWith("RIG:INSTALL_ERR")) {
          char errBuf[24];
          snprintf(errBuf, sizeof(errBuf), "%.20s", rxLine.c_str() + 4);
          hidSetStatus(0, "ERR: install", COL_ERR);
          hidSetStatus(1, errBuf,         COL_ERR);
          toastError("Install failed");
          if (cli) hidCliCleanup(mounted, weLoggedIn);
          hidWipePass();
          mscEject();
          hidInstallActive = false;
          return;
        }
        rxLine = "";
      } else if (c != '\r') {
        rxLine += c;
      }
    }
    delay(50);
  }

  if (cli) hidCliCleanup(mounted, weLoggedIn);
  hidWipePass();
  mscEject();
  hidSetStatus(0, "Timeout!", COL_ERR);
  hidSetStatus(1, "Check Pi screen", COL_ERR);
  hidInstallActive = false;
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
