# MyPi-Companion (HMS Rig Console)

ESP32-S3 touch companion for Raspberry Pi rigs: network/Wi-Fi configuration, a web-based
serial terminal, and a GUI macro & menu editor — all served from the device itself.

## Why this project exists

Raspberry Pi based rigs are often set up and moved around without a monitor, keyboard or a
known network. The everyday problems are always the same:

- **Which network is the Pi on, and what is its IP address?** Without a screen there is no
  easy way to tell.
- **Moving the Pi to another Wi-Fi network** normally requires SSH (which needs the network
  you are trying to change) or plugging in a screen and keyboard.
- **Getting a shell on the Pi** when the network is down or unknown.
- **Repeating the same admin commands** on many rigs, typed by hand every time.

The HMS Rig Console solves this with a small touch device that is plugged into the Pi's USB
port. It needs nothing from the Pi's network: everything runs over the USB cable, and the
device has its own Wi-Fi (station or access point) for the web interface on a laptop or phone.
It can even install its own Pi-side helper software over USB — no internet, no SSH.

## Features

**On the device (touch screen, 172×320, swipe + tap)**
- Status screen: the Pi's Wi-Fi SSID and IP, plus the ESP32's own SSID and IP.
- Switch the Pi between its saved NetworkManager Wi-Fi profiles.
- Wi-Fi scan (Pi side) and password entry with an on-screen character picker.
- 10 local NVS Wi-Fi profiles for the ESP32 itself (DHCP or static IP), auto-connect in order.
- Access point on demand (`RigSetup`, `192.168.4.1`) for configuring from a laptop.
- System info: uptime, heap, Pi link status, USB port, firmware version.

**Web interface (served by the ESP32, STA or AP mode, mDNS `rigconsole.local`)**
- Dashboard with live Pi + ESP status.
- Pi network management: list, connect and add Wi-Fi profiles (incl. static IP) on the Pi.
- NVS profile manager for the ESP32's own Wi-Fi slots.
- Full **xterm.js terminal** to a Pi shell over USB serial (PTY on the Pi, base64 over
  WebSocket ↔ serial) — works even when the Pi has no network.
- **Macros, menus and Quick Buttons**: GUI editor at `/macros` with a searchable macro
  library, drag/click into menus and submenus, 25 Quick Buttons, JSON import/export.
  58 ready-made Raspberry Pi macros in [`examples/`](examples/).
- FFat file management (upload, list, delete, serve).
- Web OTA firmware update at `/update`.

**Pi integration over a single USB cable (TinyUSB composite: CDC + HID + MSC)**
- **CDC serial** (`/dev/ttyACM*`, 115 200 baud) — line-based protocol to `rig_daemon.py`.
- **One-click "Install on Pi"**: the ESP32 acts as a USB keyboard (HID), opens a terminal
  and types the install command. It auto-detects the Pi's keyboard layout (en_US, sv_SE,
  de_DE, fr_FR, es_ES) by typing a probe in each layout and listening for the answer.
- **Virtual USB disk (MSC)** `RIGSETUP` carries `install.sh`, the daemon and an offline
  pyserial wheel, so installation needs no internet. Progress (`RIG:INSTALL_*`) is shown on
  the display, and the console connects automatically when the install is done.
- The daemon reports the real Wi-Fi SSID (`STATUS:SSID`) separately from the
  NetworkManager connection name (`STATUS:CONN`), so the display shows what the user
  expects while profile switching still uses the correct connection name.

## How it works

```
 Laptop / phone ──Wi-Fi (STA or AP)──▶ ESP32-S3 Rig Console ──USB──▶ Raspberry Pi
   browser: dashboard,                  touch UI, web server,         rig_daemon.py (systemd)
   terminal, macros, OTA                NVS + FFat storage            nmcli, PTY shell
                                        CDC serial  ◀──────────────▶  /dev/ttyACM*
                                        HID keyboard ──types──────▶   install command
                                        MSC disk ────mounted──────▶   install.sh + daemon
```

## Hardware

- **Board:** Waveshare ESP32-S3-Touch-LCD-1.47 (JD9853 172×320 display, AXS5106L touch)
- **Link to Pi:** USB (CDC serial + HID keyboard + MSC disk), Pi side handled by `rig_daemon.py`

## Repository layout

```
MyPi-Companion/
├── HMS_RigConsole_v5/        Arduino sketch folder (open the .ino in Arduino IDE)
│   ├── HMS_RigConsole_v5.ino
│   ├── gen_terminal_html.py  generator for terminal_html.h
│   ├── msc_image.h           generated from pi/ by tools/gen_msc_image.py
│   └── ...
├── pi/                       Pi-side sources: install.sh, uninstall.sh, rig_daemon.py,
│                             rig-console.service, offline pyserial wheel
├── tools/gen_msc_image.py    packs pi/ into the FAT12 disk image msc_image.h
├── examples/                 example macros & menus (importable via /macros)
├── docs/                     project documentation
│   ├── PROJECT_STATUS.md     architecture, protocol, routes, constraints
│   ├── GUIDELINES.md         working rules and hard technical constraints
│   └── ROADMAP.md            done / next / long-term
└── README.md
```

The sketch folder name must match the `.ino` file name (Arduino requirement).

## Build & flash (Arduino IDE)

Requirements (versions verified by the CI build):

| Component | Version | Install via |
|---|---|---|
| esp32 by Espressif (board core) | 3.3.12 | Boards Manager |
| GFX Library for Arduino (moononournation) | 1.6.9 | Library Manager |
| WebSockets (Markus Sattler) | 2.7.2 | Library Manager |

All other libraries (WiFi, WebServer, FFat, USB HID/MSC, Preferences) ship with the core.

Tools menu:

| Setting | Value |
|---|---|
| Board | **ESP32S3 Dev Module** |
| USB Mode | **USB-OTG (TinyUSB)** — required for HID + MSC |
| USB CDC On Boot | **Enabled** |
| PSRAM | **OPI PSRAM** |
| Flash Size | **16MB (128Mb)** |
| Partition Scheme | **16M Flash (2MB APP/12.5MB FATFS)** |

1. Open `HMS_RigConsole_v5/HMS_RigConsole_v5.ino`, set the Tools menu as above.
2. Compile & upload over USB.
3. Later updates can be flashed from the web page `/update` (OTA) using the
   `.bin` from the CI build or from *Sketch → Export Compiled Binary*.

> **The first v5 flash must be done over USB.** v5 changes the partition table
> (the old `partitions.csv` was removed) and OTA cannot change the partition table.
> Use `HMS_RigConsole_v5.ino.bin` for OTA — never `*.merged.bin` (that is a full
> flash image for USB/esptool at address 0x0).

## Automatic build (GitHub Actions)

Every push and pull request is compiled by `.github/workflows/build.yml` with the
settings above. To download the firmware: **Actions** → latest *Build firmware* run →
**Artifacts** → `firmware-<sha>` (zip containing the `.bin` files).
The build also fails if `terminal_html.h` is out of date with `gen_terminal_html.py`.

## Generated files

`terminal_html.h` is generated — **do not edit it by hand**. Edit `gen_terminal_html.py`
and run:

```bash
cd HMS_RigConsole_v5
python3 gen_terminal_html.py
```

Commit both files. The generated header is kept in the repo so the sketch compiles
without Python.

`msc_image.h` is also generated — it is the FAT12 image of the `RIGSETUP` USB disk.
Edit the files in `pi/` and run from the repo root:

```bash
python3 tools/gen_msc_image.py
```

Commit the `pi/` changes together with the regenerated `msc_image.h`.

## Web interface

| Path        | Purpose                                  |
|-------------|------------------------------------------|
| `/`         | Dashboard                                |
| `/networks` | Pi network profiles                      |
| `/profiles` | NVS profile manager                      |
| `/terminal` | xterm.js serial terminal                 |
| `/macros`   | GUI macro, menu & quick-button editor    |
| `/update`   | OTA firmware upload                      |

Default access point: SSID `RigSetup`, IP `192.168.4.1`.

## Example macros

See [`examples/README.md`](examples/README.md) — 58 Raspberry Pi macros in 5 menu groups.

## Version history

- **v5** — GUI macro/menu editor at `/macros`; terminal menu links to it; fixed web OTA
  (multipart upload); removed `partitions.csv` so FFat mounts; keyboard layout
  auto-detection for Install on Pi; auto-connect after install; real SSID vs. NM
  connection name (`STATUS:CONN`); Pi-side sources in `pi/`; CI build on GitHub Actions
- **v4** — baseline import, locked. Available as git tag `v4`
  (GitHub → Tags → `v4` → download ZIP, or `git checkout v4`).

## Project context (quick start for new contributors and AI assistants)

Read this section plus [`docs/GUIDELINES.md`](docs/GUIDELINES.md) and
[`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) before making changes.

- **Language:** discussion may be in Swedish; everything committed (code, comments, UI text,
  docs, macro names) is US English.
- **Workflow:** feature branch (`feature/…`, `fix/…`, `docs/…`, `pi/…`) → pull request →
  reviewed and merged by the owner. Never push directly to `main`.
- **Minimal changes:** read the affected code first, change only what is needed.
- **Generated files — never edit by hand:** `terminal_html.h` (from
  `gen_terminal_html.py`) and `msc_image.h` (from `pi/` via `tools/gen_msc_image.py`).
  `page_html.h`, `profiles_html.h` and `macros_html.h` are hand-edited.
- **Hard rules:** no FitAddon (use `termFit()`); Quick Buttons are a vertical list;
  stop the web server only with `webServerStop()`; no `partitions.csv` in the sketch folder;
  `Serial.setRxBufferSize(4096)` before `Serial.begin()`.
- **Flashing:** the first v5 flash must be over USB (partition table change); OTA uses
  `HMS_RigConsole_v5.ino.bin`, never `*.merged.bin`.
- **Naming pitfall:** on the ESP32, `activeSSID` is the real Wi-Fi SSID (for display) and
  `activeConn` is the NetworkManager connection name (for matching and `CMD:CONNECT`).
  The Pi profile list (`PROFILES:` / `piProfiles[].ssid`) contains NM connection names,
  despite the field name.
- **Pi keyboard:** HID sends key positions, not characters. `hid.ino` probes en_US, sv_SE,
  de_DE, fr_FR and es_ES and uses the layout that answers; the reference Pi uses sv_SE.
- **Status and plans:** open items in [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md)
  (v5 status) and [`docs/ROADMAP.md`](docs/ROADMAP.md).
