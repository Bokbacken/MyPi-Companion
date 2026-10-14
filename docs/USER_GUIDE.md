# MyPi Companion — User Guide

_Firmware v6.3.3 · October 2026_

MyPi Companion is a small touch-screen device that plugs into a Raspberry Pi over USB.
You can use it to see and change the Pi's network setup, open a terminal on the Pi from a
browser, and install the Pi-side software, all without a monitor or keyboard on the Pi.

---

## Contents

1. [Overview](#1-overview)
2. [Hardware and first start](#2-hardware-and-first-start)
3. [Display layout](#3-display-layout)
4. [Status bar](#4-status-bar)
5. [Touch interface and gestures](#5-touch-interface-and-gestures)
6. [Touch menu screens](#6-touch-menu-screens)
7. [Character picker keyboard](#7-character-picker-keyboard)
8. [Web interface](#8-web-interface)
9. [Access point (AP) mode](#9-access-point-ap-mode)
10. [Install on Pi](#10-install-on-pi)
11. [OTA firmware update](#11-ota-firmware-update)
12. [NVS profile slots](#12-nvs-profile-slots)
13. [SD card and storage](#13-sd-card-and-storage)
14. [Macros and quick-action buttons](#14-macros-and-quick-action-buttons)
15. [Troubleshooting](#15-troubleshooting)

---

## 1. Overview

The Companion is an ESP32-S3 with a 172×320 capacitive touch screen. A single USB cable
to the Pi gives it three roles at once:

| USB role | What it does |
|---|---|
| **Serial (CDC-ACM, 115200 8N1)** | Talks to the `rig-console` daemon on the Pi (`/dev/ttyACM0`) |
| **HID keyboard** | Types the installer command on the Pi during *Install on Pi* |
| **Mass storage (MSC)** | Shows up as the `RIGSETUP` disk with the installer files, or shares the SD card |

The device also connects to Wi-Fi on its own (or starts its own access point) and runs a
web server, so you can use all of its functions from a phone or laptop browser.

What you can do with it:

- See the IP addresses of the ESP and the Pi at a glance
- Switch the Pi between its saved NetworkManager Wi-Fi connections
- Add new Wi-Fi networks to the Pi, including the password, from the touch screen
- Open a bash terminal on the Pi in a browser (xterm.js, up to 4 sessions)
- Run saved command macros with one tap
- Start an access point with optional internet sharing (NAT)
- Install the Pi-side daemon from scratch with no keyboard attached to the Pi
- Update the firmware over the air

---

## 2. Hardware and first start

| Item | Value |
|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-1.47 |
| Display | JD9853, 172×320 px, portrait |
| Touch | AXS5106L capacitive (I2C address 0x63) |
| Buttons | None, the device is touch-only |
| Default AP | SSID `RigSetup`, password `MyPi1234` |
| mDNS name | `rigconsole.local` |

### First start

1. Connect the Companion's USB-C port to a USB port on the Raspberry Pi. The Pi powers
   the Companion.
2. The splash screen shows the MyPi Companion logo while the ESP starts a background
   Wi-Fi scan.
3. The home screen appears with the status bar at the bottom:
   - If the ESP finds a network that is saved in one of its profile slots, it connects
     and shows its IP in large text.
   - If the Pi daemon is not installed yet, the Pi row reads **`Pi: offline`** in red. See
     [Install on Pi](#10-install-on-pi).
4. Once the daemon is running, the Pi row turns green and shows the Pi's interface and IP.

The ESP sends `HELLO` every 2 seconds while the Pi has the serial port open but has not
replied yet. This means the Companion connects by itself as soon as the Pi has booted.
You do not need to unplug and reconnect it.

---

## 3. Display layout

The screen is split into four fixed horizontal zones:

```
 y=0   ┌──────────────────────┐
       │ Header     (AP)[logo]│  20 px   screen title, AP icon (when on), MyPi icon
 y=20  ├──────────────────────┤
       │                      │
       │   Content area       │  216 px  menus / info / keyboard
       │                      │          (menu rows are 18 px high)
       │                      │
 y=236 ├──────────────────────┤
       │   Status bar         │  64 px   ESP + Pi network status
 y=300 ├──────────────────────┤
       │ Touch bar            │  20 px   2 or 3 labeled tap zones
 y=319 └──────────────────────┘
```

| Zone | Rows (y) | Height | Content |
|---|---|---|---|
| Header | 0–19 | 20 px | Name of the current screen, MyPi logo icon at top right. While the AP runs, a Wi-Fi AP icon appears in the header, left of the logo, with an orange **`+`** next to it when NAT is on. Tap the header to hide or show the AP page (see [section 9](#hiding-and-showing-the-ap-page)) |
| Content | 20–235 | 216 px | Current menu, info page or keyboard |
| Status bar | 236–299 | 64 px | Network status, always visible (dark background) |
| Touch bar | 300–319 | 20 px | Labels of the tap zones |

**Messages (toasts).** Short confirmations and errors appear as a one-line message at
the bottom of the content area (y = 221–234), just above the status bar. Orange text is
information (shown for 5 s), red text is an error (shown for 8 s). A new message replaces
the old one, and a message stays visible when you change screen. During *Install on Pi*
each install step is shown here until the next step replaces it.

---

## 4. Status bar

The status bar is always visible. It shows the ESP's network status on top, a thin grey
line, and the Pi's status underneath.

| Offset from top | Content | Style |
|---|---|---|
| +2 … +12 | UART 8N1 waveform of the character `C` | Green, decorative |
| +14 | **ESP IP address**, e.g. `192.168.1.42` | Large text (a full `255.255.255.255` fits); `no Wi-Fi` when not connected |
| +32 | ESP Wi-Fi SSID | Small, dim |
| +41 | Separator line | Grey, ESP above / Pi below |
| +44 | `Pi <iface>: <ip>`, e.g. `Pi wlan0: 10.0.0.3` | Green when connected; red **`Pi: offline`** when not |
| +55 | Pi SSID / info / error | See below |

**Multiple Pi interfaces.** If the Pi has more than one active IPv4 interface (for
example `eth0`, `eth1` and `wlan0`), the `Pi <iface>: <ip>` row shows each one for
3 seconds in turn. With a single interface the row does not change. Loopback is never
shown.

**The bottom row (+55)** shows the first of these that applies:

1. A **red error message** from the Pi, e.g. a failed connect
2. The **Pi SSID**, when the interface shown in the row above is wireless (`wl…`)
3. Blank for wired interfaces

Confirmations no longer appear here; since v6.3 they are shown as toasts (see
[section 3](#3-display-layout)).

When the Pi is offline the bottom row reads `Connect Pi via USB`.

---

## 5. Touch interface and gestures

There are two kinds of input:

- **Tap**: touch inside the touch bar (y ≥ 300) and lift without moving more than 14 px.
  The only other tap zone is the **header** (y < 20): while the AP runs, a header tap
  hides or shows the AP page. Taps anywhere else on the screen are ignored.
- **Swipe**: move at least 22 px along one axis. Swipes work **anywhere** on the screen,
  and the axis with the larger movement decides the direction.

### Touch bar zones

| Mode | Left | Middle | Right |
|---|---|---|---|
| Standard (2 zones) | NEXT / BACK / STOP … | (part of left/right) | OPEN / SELECT / START … |
| Keyboard (3 zones) | **NEXT**: insert character | **DEL**: delete | **OK**: save and connect |

The labels change with the screen, so always check the touch bar to see what a tap does.

### Gestures in all standard menus

| Gesture | Action |
|---|---|
| Swipe **up** | Previous item |
| Swipe **down** | Next item |
| Swipe **left** | Go back |
| Swipe **right** | Open / select |
| Tap **left** zone | Next item (same as swipe down) |
| Tap **middle** zone | Next item |
| Tap **right** zone | Open / select |

### Special screens

| Screen | Gesture | Action |
|---|---|---|
| AP active | Tap left or swipe left | **Stop the AP** |
| Any screen except the keyboard, while the AP runs | Tap the **header** | Hide / show the AP page (the AP keeps running) |
| Advanced → Install GUI-Pi confirm prompt | Tap right or swipe right | Start the install; any other gesture cancels |
| System info | Tap left or swipe left | Back |
| Keyboard | See [section 7](#7-character-picker-keyboard) | |

---

## 6. Touch menu screens

```
Home (status)
└── Main Menu
    ├── Pi networks            → switch the Pi's active connection
    ├── ESP Wi-Fi              → 10 slots + ESP Scan… + Pi Scan…
    │   ├── ESP Scan…  → pick SSID → Keyboard
    │   └── Pi Scan…   → pick SSID → Keyboard
    ├── Local profiles (→ Pi Profile) → push a saved slot to the Pi
    └── Advanced
        ├── Start AP           → AP profile picker → AP active
        ├── Show info          → System info
        ├── Install CLI-Pi     → Keyboard (username) → Keyboard (password) → install
        ├── Install GUI-Pi     → confirm prompt → install
        ├── Flip display
        ├── Boot AP: <profile> → Boot AP picker (None / profile 1–4)
        └── Back
```

### Home

The start screen shows **MyPi Companion**, the Pi link status and the serial speed
(115200). Swipe right or tap the right zone to open the main menu.

### Main Menu

Four items: **Pi networks**, **ESP Wi-Fi**, **Local profiles** and **Advanced**.

### Pi networks

Lists the NetworkManager connections saved on the Pi, as reported by the daemon. The
connection that is active right now has a **green dot**. Select a connection to make the
Pi switch to it (`CMD:CONNECT`). The result appears in the status bar.

### Local profiles (→ Pi Profile)

Shows the 10 [NVS profile slots](#12-nvs-profile-slots). Select a filled slot to send its
full settings (SSID, password, DHCP or static IP) to the Pi (`CMD:ADDWIFI_FULL`). The Pi
then saves it as a new NetworkManager connection. This is the easiest way to give the Pi
a network that the ESP already knows.

### ESP Wi-Fi

Lists the 10 NVS slots, followed by **ESP Scan…**, **Pi Scan…** and **Back**.

- **Select a filled slot**: the ESP starts connecting to that network. The status bar shows
  `joining..` with the SSID until it is connected (or the attempt times out). The screen
  stays responsive meanwhile, and a running AP stays up.
- **ESP Scan…**: the ESP scans with its own radio. It lists up to 20 SSIDs, **2.4 GHz
  only** (the ESP32-S3 has no 5 GHz radio).
- **Pi Scan…**: the Pi scans with `nmcli`, which finds **both 2.4 and 5 GHz** networks.

Selecting an SSID in either scan list opens the [keyboard](#7-character-picker-keyboard)
so you can enter the password. **OK** saves the network to a slot and connects.

### Advanced

| Item | What it does |
|---|---|
| **Start AP** | Opens the AP profile picker (profiles 0–3); selecting one starts the AP |
| **Show info** | System information screen |
| **Install CLI-Pi** | Asks for the Pi username and password on the keyboard, then runs the CLI/headless installer (see [section 10](#10-install-on-pi)) |
| **Install GUI-Pi** | Shows a confirm prompt, then runs the Desktop installer (see [section 10](#10-install-on-pi)) |
| **Flip display** | Rotates the screen 180° |
| **Boot AP** | Opens a list where you choose which AP profile starts automatically at power-on, or **None** (see below); saved per profile in NVS |
| **Back** | Back to the main menu |

#### Boot AP picker

The **Boot AP** item shows the current choice, for example `Boot AP: none`,
`Boot AP:<name>`, or `Boot:<name>+Hide` when that profile also has **Hide on boot** set.
The label is green while a boot profile is set and dim when it is `none`. Long profile
names are shortened to fit.

Selecting it opens the **Boot AP** list:

| Item | Meaning |
|---|---|
| `None` | No AP starts at boot |
| `1: <name>` … `4: <name>` | That profile starts at boot |

The current choice is shown in green, and the cursor starts on it. Swipe up/down (or tap
NEXT) to move, and tap **SELECT** or swipe right to save and return to Advanced. Swipe
left to go back without changing anything.

Only one profile can start on boot; choosing a profile clears the flag on the others.
**Hide on boot** is not changed from this list; set it per profile on `/ap-profiles`.
The choice is stored in the AP profiles themselves (NVS), so it is the same setting as
the **Start on boot** checkbox on `/ap-profiles`.

### AP active

Shown while the access point is running. It displays the AP SSID, password, the IP of
the web interface, NAT status and the connected clients. The list header shows the
count and the limit, e.g. `Clients (4/8):`. There is room for 4 rows; with more than 4
clients the screen shows 3 and a `+N more (web)` row, and `/ap-profiles` lists them all
(see [Client limit](#client-limit)). **Tap left or swipe
left to stop the AP.** Tap the header to hide the page; the AP keeps running (see
[section 9](#hiding-and-showing-the-ap-page)).

Each client row shows the last IP octet and the best name the ESP can find:

1. **Host name**, from the first source that has one:
   - the companion Pi (its IP matches an interface the daemon reports);
   - the name the device sends when it asks for an IP (DHCP). Windows always sends
     it; Android sends it when *Send device name* is on in the Wi-Fi network
     settings. The ESP only sees it when the device connects or renews its lease,
     so a device that was already connected when the ESP started shows its name
     after it reconnects;
   - an mDNS reverse lookup (Linux with Avahi, macOS, iOS);
   - a NetBIOS name query (Windows on a Private network, Samba).

   Names usually appear a few seconds after the client gets its IP. The ESP retries
   for devices that answer later.
2. **Manufacturer** from the MAC address (Raspberry Pi, Apple, ...).
3. **The last 3 bytes of the MAC.**

Windows blocks mDNS and NetBIOS replies on networks marked *Public*, and Android
does not answer either lookup, so for those devices the DHCP name is the one
that works. A device that sends no DHCP name and answers no lookup shows the MAC.

### System info

Two pages. Tap right (or swipe right/up/down) to switch page; tap left to go back.

**ESP32 Info** — firmware version and build date (e.g. `6.3` and the compile date), uptime,
free heap, Pi link state, the Pi's USB port (e.g. `ttyACM0`), ESP IP, sketch size and
free OTA space.

**Pi Info** — reported by the Pi daemon every poll: hostname, model, OS (codename and
32/64-bit), kernel, daemon version, Pi uptime, CPU temperature (orange from 70 °C, red
from 80 °C), load, RAM, disk (root file system), power (under-voltage / throttling, now or
since boot), Wi-Fi signal (dBm) and whether the clock is NTP-synchronized. Values turn grey
when the Pi link is offline or the data is older than 20 s.

If the page says **daemon too old – reinstall**, the Pi runs a daemon from before v6.3:
run Install CLI-Pi / Install GUI-Pi again (or reinstall from the `RIGSETUP` disk).

The same information is shown on the web page `/info` (cards *ESP32* and *Raspberry Pi*).

---

## 7. Character picker keyboard

Without a physical keyboard, passwords are entered with a character wheel.

```
┌──────────────────────┐
│ az AZ 09 S1 S2 I1 I2 │  ← range tabs (current tab highlighted)
│ secretk|             │  ← password field with cursor (plain text)
│         h            │  -2
│         i            │  -1
│       [ j ]          │  ← current character (large, highlighted)
│         k            │  +1
│         l            │  +2
└──────────────────────┘
  NEXT  |  DEL  |  OK
```

The wheel shows 5 rows: two characters before the current one, the current character
(large and highlighted), and two after. It wraps around at both ends. When the password
is longer than 22 characters, the field shows `...` followed by the last 19 characters.
The text is shown in plain text, not masked, so you can check what you typed. This also
applies to the Pi password in **Install CLI-Pi**, so make sure nobody else can see the
screen.

### Controls

| Input | Action |
|---|---|
| Swipe **up** | Next character. A longer swipe moves more steps, up to 5. |
| Swipe **down** | Previous character |
| **Vertical drag** | Turns the wheel in real time while your finger moves |
| Swipe **left** | Previous range tab |
| Swipe **right** | Next range tab |
| Tap **NEXT** (left) | Insert the highlighted character |
| Tap **DEL** (middle) | Delete the last character |
| Tap **OK** (right) | Save the profile and connect |

### Range tabs

| Tab | Contents |
|---|---|
| `az` | a–z (26) |
| `AZ` | A–Z (26) |
| `09` | SPC (space), 0–9 (11) |
| `S1` | `! # @ $ % & * - _ + = . , ? : ; /` (17) |
| `S2` | `` " ' ( ) [ ] \ { } ^ ~ \| < > ` € £ ¥ ¤ § `` (20) |
| `I1` | Nordic + German/Dutch: `å ä ö æ ø ü ë ï é ß ð þ Å Ä Ö Æ Ø Ü É Ð Þ` (21) |
| `I2` | French/Spanish/Portuguese/Italian: `à â ç è ê î ô û ù ÿ œ á í ó ú ý ñ ã õ ì ò Ç Ñ ¿ ¡` (25) |

**Space** is the first entry of the `09` tab and is shown as `SPC` on the wheel. `S1`
holds the symbols most used in passwords, and `S2` the brackets and rarer symbols. Every
printable ASCII symbol is in exactly one tab, and no character appears twice in any tab.
`I1` covers Swedish, Norwegian, Danish, Finnish, Icelandic, Faroese, German and Dutch;
`I2` covers French, Spanish, Portuguese and Italian. Uppercase letters that are rare in
passwords (for example `À`, `Ë`) are left out to keep the wheels short.

All characters are shown as real letters on the display. Most come from the built-in
font; `ø Ø ð Ð þ Þ œ ý ã õ € ¤` are not in it and are drawn as small custom bitmaps.
When you press **OK** the password is converted to **UTF-8** before it is stored or used.
That is the encoding WPA2 and the Pi login expect, so a password with `å`, `ø` or `€`
entered on the touch screen works the same as one entered on the web.

> **Limitation:** **Install CLI-Pi** types the username and password into the Pi over the
> USB keyboard, and that path only supports ASCII. Non-ASCII characters (from `I1`, `I2`
> and the currency signs in `S2`) are skipped there. Use them for Wi-Fi passwords only.

---

## 8. Web interface

The web server runs whenever the ESP has a network, both on Wi-Fi (STA) and in AP mode.
Open one of these:

- `http://<ESP IP>/` (or `http://<ESP IP>/index.html`, same page), using the large IP
  shown in the status bar
- `http://rigconsole.local/` (mDNS)
- In AP mode: `http://192.168.4.1/` (or the `.1` address of the subnet set in the AP
  profile)

### Functions on both the touch display and the web

| Function | Touch screen | Web page |
|---|---|---|
| Pi networks: switch the Pi's active Wi-Fi | Main Menu → Pi networks | `/networks` (alias `/pinetworks`) |
| ESP Wi-Fi: manage slots, push to the Pi | Main Menu → ESP Wi-Fi / Local profiles | `/profiles` |
| Connect the ESP to a network (scan, SSID, saved slot) | Main Menu → ESP Wi-Fi | `/profiles` → **ESP32 Wi-Fi Connection**, or **ESP** on a slot |
| Install on Pi | Advanced → Install CLI-Pi / Install GUI-Pi | `/installonpi` (Desktop and CLI) |
| AP mode | Advanced → Start AP | `/ap-profiles` (edit and start) |
| AP start on boot | Advanced → Boot AP | `/ap-profiles` → **Start on boot** / **Hide on boot** |
| System info, firmware version and build date | Advanced → Show info | `/info` |

### Web only

| Page | Description |
|---|---|
| `/` | Dashboard: quick-action buttons, Pi link status, ESP info |
| `/terminal` | xterm.js terminal on the Pi's bash. Each browser tab gets its own session (up to 4); command history is shared between tabs (Ctrl+Up/Down) |
| `/macros` | Manage terminal macros and quick-action buttons |
| `/storage` | File manager for internal flash and the SD card |
| `/ap-profiles` | Edit the 4 AP profiles (name, SSID, password, subnet, NAT, start on boot, hide on boot). The cards are shown two per row (one per row on a phone). A saved password shows as dots; the eye button shows the real password. The status table shows the client count against the limit (e.g. `4/8 client(s)`, `(full)` at the limit) and lists each connected AP client with the last IP octet (e.g. `.2`), the manufacturer when known (Raspberry Pi, Apple, …) or the end of its MAC, and the full MAC. A host name, when found, replaces the manufacturer (which moves into brackets); hover the MAC to see how the name was found |
| `/update` | OTA firmware update |

### API endpoints (for scripts)

| Method | Endpoint | Returns / does |
|---|---|---|
| POST | `/addwifi` | Adds a Wi-Fi profile to the Pi over serial |
| GET | `/connect?profile=<name>` | Tells the Pi to connect to a named NM connection |
| GET | `/api/piconns` | JSON array of the Pi's NM connection names |
| GET | `/api/nvs` | JSON of all 10 NVS slots |
| POST | `/api/espconnect` | Connect the ESP itself: `slot=<0-9>`, or `ssid=…&pass=…` (saved to a slot first) |
| GET | `/api/espstatus` | ESP Wi-Fi state: connected / connecting, SSID, IP, dBm, channel, last result |
| POST/GET | `/api/espscan` | Start an ESP scan / read the results |
| POST | `/api/espdisconnect` | Drop the ESP's Wi-Fi link (only while the AP runs) |
| GET | `/api/info` | System info JSON, incl. `pi_sys` (Pi key/value list) and `pi_sys_age` |
| GET | `/api/installstatus` | Live install progress (polled by `/installonpi`) |
| GET | `/api/sd/status` | SD card status (present, shared, …) |
| — | `/api/rigsetup/*` | Manages the `RIGSETUP` USB disk shown to the Pi |

---

## 9. Access point (AP) mode

The Companion can run its own Wi-Fi network. This is useful when there is no network
nearby, or when you want to share one Wi-Fi connection with a laptop.

### AP profiles

There are **4 profiles** (0–3). Each one has:

| Field | Notes |
|---|---|
| Name | Shown in the touch picker |
| SSID | Network name |
| Password | At least 8 characters |
| Subnet | One of four presets: `192.168.4.x`, `192.168.42.x`, `10.11.0.x`, `172.16.4.x` |
| NAT | Internet sharing on/off |
| Start on boot | Start this profile automatically at power-on. Only **one** profile can have it |
| Hide on boot | Hide the AP page right after the boot start. Set separately for each profile |

The first profile defaults to SSID `RigSetup`, password `MyPi1234`. Change it on
`/ap-profiles`.

### Starting and stopping

- **Touch:** Advanced → Start AP → select a profile. The *AP active* screen appears. Tap
  left or swipe left to stop.
- **Web:** `/ap-profiles` → start the selected profile.
- **At boot:** choose a profile with **Advanced → Boot AP** on the touch screen, or tick
  **Start on boot** on that profile's card on `/ap-profiles` and press **Save**. The
  setting is stored per profile in NVS. At power-on the Companion starts that profile
  and shows the *AP active* screen. If **Hide on boot** is also set for the profile, it
  goes straight to the status screen instead, with the AP icon in the header.

#### Start on boot and Hide on boot on the web

Each card on `/ap-profiles` has two checkboxes above the **Save** button:

- **Start on boot** is exclusive. Ticking it on one card unticks it on the other cards,
  and when you save, the firmware clears the flag on every other profile.
- **Hide on boot** is independent for each profile. It only has an effect for the
  profile that has **Start on boot**.

The touch setting **Advanced → Boot AP** and the web checkboxes change the same stored
values.

### Hiding and showing the AP page

While the AP runs, you can **tap the header** to hide the *AP active* page and use the
other menus. **The AP keeps running** and the web server stays reachable. The header
then shows a small AP icon, left of the MyPi logo, with an orange **`+`** next to it
when NAT is on. Tap the header again to go back to the *AP active* page. Header taps are
ignored on the keyboard screen, so a half-typed password is never lost.

### Client limit

The AP accepts at most **8 clients** at a time (phones, laptops, tablets and the Pi all
count). A 9th device is refused when it tries to join and keeps trying, or uses another
network. Disconnect a device you no longer need to make room.

Why 8: the ESP32 Wi-Fi driver itself can serve about 10 stations, but the DHCP server in
the ESP32 core keeps track of at most 8, so a 9th or 10th device would join without
getting an IP address. Firmware older than v6.3.3 used the library default of **4**.

All clients share the ESP's single radio. With NAT on they also share the few Mbit/s of
software NAT, so many busy clients make every client slower.

### NAT / internet sharing

With NAT on, the ESP routes AP clients through its own Wi-Fi uplink (NAPT).

- AP clients always get DNS **1.1.1.1**, because upstream DNS servers (for example on
  hotel networks) are often private and cannot be reached through NAPT.
- Since v6.1 the AP uses **the same channel as the uplink and 20 MHz bandwidth**, and the
  uplink is locked to HT20. This keeps the radio from switching between channels.
- Throughput is limited by software NAT on the ESP32, a few Mbit/s. That is enough for
  SSH, the terminal and web pages, but not for large downloads.

---

## 10. Install on Pi

This installs the `rig-console` daemon on a Pi that does not have it yet. The Companion
acts as a USB keyboard and types the installer command. The installer files come from
the `RIGSETUP` USB disk that the Companion presents to the Pi.

The keyboard layout of the Pi is detected automatically: a test command is typed in
several layouts, and the one that answers over serial is used.

### Desktop mode

For Raspberry Pi OS with a desktop and a logged-in user.

1. The Companion presses **Ctrl+Alt+T** to open a terminal window.
2. It types the install command, which runs `install.sh` from the `RIGSETUP` disk.
3. Progress appears on `/installonpi` and as a message line on the display (each step
   replaces the previous one, then a final done or error message).
4. When the installer reports `RIG:INSTALL_DONE`, the Companion ejects the disk, waits
   3 seconds and sends `HELLO`. The Pi row then turns green with no reboot needed.

No login details are needed. Desktop mode can be started from the web page or from the
touch screen with **Advanced → Install GUI-Pi**. The touch screen first shows a confirm
prompt (`Install GUI-Pi?`). Tap right or swipe right to start; any other tap or swipe
cancels.

### CLI / headless mode

For Raspberry Pi OS Lite, or any Pi that boots to a text console.

You can start it from the web page or from the touch screen.

**From the web page:**

1. Open **`/installonpi`** in a browser.
2. Choose **CLI**, enter the Pi **username and password**, and start.

**From the touch screen:**

1. Select **Advanced → Install CLI-Pi**.
2. The keyboard opens with the prompt **Pi username**. Enter the username and tap **OK**.
3. The keyboard opens again with the prompt **Pi password**. Enter the password and tap
   **OK**. The password is shown in plain text while you type it, not masked.
4. The install starts right away.

In both cases the Companion then logs in on `tty1`, authenticates `sudo`, mounts
`RIGSETUP` by label, runs the installer and logs out.

The password is held in RAM only. It is never saved to flash and is cleared after each
attempt, so you must enter it again for every install. If an install is started in CLI
mode without credentials loaded, the display shows **"Start from web"**; use
**Install CLI-Pi** on the touch screen or `/installonpi` to enter them.

The mode, keyboard layout and username are saved (NVS `rigcfg`). The password is not.

---

## 11. OTA firmware update

> ⚠️ **Only ever flash the `.ino.bin` file.** Never upload `*.merged.bin` over OTA. The
> merged image includes the bootloader and partition table and **erases NVS**, which
> deletes all saved Wi-Fi slots, AP profiles, quick buttons and install settings.

1. Get `MyPi_Companion_V6.ino.bin` from the CI build artifacts or from your own build.
2. Open `http://<ESP IP>/update`.
3. Drag and drop the `.ino.bin` file (or choose it) and start the upload.
4. The web page shows the upload progress in percent. When the upload is complete the bar
   switches to a moving bar with *Flashing, please wait...* while the ESP finishes writing and restarts.
   The display shows a full-screen progress bar at the same time. The device restarts by
   itself when the update is done; reload the page after about 10 seconds.
5. Check the new version on `/info` or **Advanced → Show info**.

OTA replaces only the application. Changes to the partition table need a USB flash from
the Arduino IDE or `arduino-cli`.

If the Pi-side files changed in the new firmware, run **Install on Pi** again or
reinstall the daemon on the Pi to update it too.

---

## 12. NVS profile slots

The Companion stores **10 Wi-Fi profile slots** in its non-volatile storage (NVS). They
survive reboots and OTA updates.

Each slot holds:

- SSID and password
- DHCP **or** a static IP (IP, subnet mask, gateway, DNS)

### Priority

**Slot 0 has the highest priority.** At boot the ESP scans and then tries the slots in
order 0 → 9. It skips SSIDs that are not visible and stops at the first network that
connects. To make a network the preferred one, move it to a lower slot number on
`/profiles`.

### Shared with the Pi

The same slots are used in two directions:

- **For the ESP itself:** auto-connect at boot, a manual connect from *ESP Wi-Fi*, or the
  **ESP** button / *ESP32 Wi-Fi Connection* card on `/profiles`.
- **For the Pi:** *Local profiles (→ Pi Profile)* on the touch screen, or the **Pi** button
  on `/profiles`, sends a slot's full settings to the Pi with `CMD:ADDWIFI_FULL`. The Pi
  saves it as a NetworkManager connection.

New networks entered with the keyboard (from ESP Scan or Pi Scan) are saved to a slot,
so they become available for both.

### Connecting the ESP from the web

On `/profiles`, the card **ESP32 Wi-Fi Connection** shows the ESP's own link (updated
automatically). **Scan** lists the networks the ESP sees (2.4 GHz only); tap one, enter the
password and press **Connect ESP to this SSID**. The network is saved to a slot (an existing
slot with the same SSID is reused, else the first free slot, else slot 10) and the ESP joins it.
A saved slot can also be joined with its **ESP** button.

Things to know:

- **With the AP running** (for example AP on boot), the AP stays up. The ESP has one radio,
  so the AP moves to the channel of the joined network; AP clients may drop for a few
  seconds and then reconnect. NAT starts automatically once the uplink is up.
- **Without the AP**, if you reached the page over the ESP's current Wi-Fi, the page is cut
  off when the ESP switches network. Reopen it on the new IP (status bar) or
  `http://rigconsole.local/`.
- **Disconnect ESP** is only offered while the AP runs, so you cannot lock yourself out.

---

## 13. SD card and storage

### Internal flash (FFat)

The internal flash has a FAT data partition of about 9 MB. It holds:

- `macros.json` and `menus.json` (macros and terminal menus)
- `/rigsetup/`, the installer files used to build the `RIGSETUP` disk
- Any files you upload

### SD card

- **FAT32** cards are supported, including 128 GB cards.
- **SD Share** exposes the SD card to the Pi as a USB mass storage disk. You can then
  move files between a browser (via `/storage`) and the Pi.
- Check the status with `/api/sd/status`.

### `/storage` file manager

Lists both internal flash and the SD card. You can **upload, download, delete and copy**
files between them.

### The `RIGSETUP` disk

A virtual FAT12 disk image that the Companion builds in PSRAM from `/rigsetup/` each
time it is shown to the Pi. It contains `install.sh`, the daemon (`daemon.py`) and an
offline `pyserial` wheel, so the install works without internet access.

---

## 14. Macros and quick-action buttons

**Macros** are saved shell commands that are sent to the Pi terminal.

- Up to **200 macros** in the pool (stored in `/config/macros.json` on the ESP's flash)
- Each has a **name** (up to 24 characters) and a **command** (up to 200 characters)
- Manage them on **`/macros`**. You can also import and export them as JSON; examples
  are in [`examples/`](../examples/).

**Quick-action buttons** on the home page (`/`) and in the terminal are linked to macro
slots. Clicking a button sends that macro's command to the Pi.

- Up to **25 quick-button slots** (stored in NVS, survive firmware updates)

There are three ways to run a macro:

- Click a quick-action button on the dashboard
- Click a quick button in the `/terminal` side panel
- Choose it from the terminal's macro menu

Macro and quick-button commands are not added to the shared terminal history. Only
commands you type yourself are stored.

---

## 15. Troubleshooting

| Symptom | Likely cause | What to do |
|---|---|---|
| **`Pi: offline`** in red | USB cable not connected (or charge-only cable), or the daemon is not installed or not running | Check the cable. Run [Install on Pi](#10-install-on-pi). On the Pi: `systemctl status rig-console` |
| Pi stays offline after the Pi reboots | The Pi is still booting | Wait. The ESP sends `HELLO` every 2 s and connects by itself once the daemon opens the port. There is no need to replug |
| Web server seems to **hang** or pages load slowly | Old browser tabs (especially `/terminal` with WebSockets) are still open and use the ESP's limited connections | Close old tabs on all devices and reload the page |
| All profiles and settings gone after an update | A `*.merged.bin` was flashed over OTA, which erased NVS | Re-enter the profiles. Always use **`.ino.bin`** for OTA |
| Network not found in ESP Scan | The network is 5 GHz only | Use **Pi Scan…**, which finds 5 GHz networks too |
| Passwords with `å`/`ä`/`ö` look wrong on the display | Firmware older than v6.2 | Update the firmware (see [section 11](#11-ota-firmware-update)). Since v6.2 the `In` tab shows these letters correctly and stores passwords as UTF-8 |
| "Start from web" on Install on Pi | CLI mode is selected but no credentials are loaded | Use **Advanced → Install CLI-Pi** on the touch screen, or `/installonpi`, and enter the username and password |
| The AP page disappeared but the AP icon is still in the header | The header was tapped, or the boot profile has **Hide on boot** set | Tap the header to show the AP page again. The AP was running the whole time |
| The wrong AP profile starts at boot | Another profile has **Start on boot** | Check **Advanced → Boot AP**, or the checkboxes on `/ap-profiles` |
| Typed install command looks garbled | Keyboard layout detection failed | Run the install again. Make sure the console or terminal has focus |
| Slow internet through the AP | Software NAT on the ESP32 | Expected (a few Mbit/s). Connect directly to the uplink for large transfers |
| A device cannot join the AP, other devices are fine | The AP already has 8 clients (`8/8` on the AP screen, `(full)` on `/ap-profiles`) | Disconnect a device you do not need. See [Client limit](#client-limit) |
| Chrome on a phone says *This site can't be reached* for `http://192.168.4.1/`, but other pages and other browsers work | Old browser data for that address, for example from another router that used the same IP. Clearing cookies and history did not always help in testing | Open `http://192.168.4.1/index.html` instead. All *back* links on the web pages go to `/index.html` |
| `rigconsole.local` does not resolve | mDNS is not supported on the client (some Android versions, some networks) | Use the IP shown in the status bar |
