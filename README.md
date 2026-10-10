# MyPi-Companion

ESP32-S3 touch companion for Raspberry Pi rigs: network/Wi-Fi configuration, a web-based
serial terminal, and a GUI macro & menu editor — all served from the device itself.

## Hardware

- **Board:** Waveshare ESP32-S3-Touch-LCD-1.47 (JD9853 172×320 display, AXS5106L touch)
- **Link to Pi:** USB (CDC serial + HID), Pi side handled by `rig_daemon.py`

## Repository layout

```
MyPi-Companion/
├── HMS_RigConsole_v5/        Arduino sketch folder (open the .ino in Arduino IDE)
│   ├── HMS_RigConsole_v5.ino
│   ├── gen_terminal_html.py  generator for terminal_html.h
│   └── ...
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

- **v5** — GUI macro/menu editor at `/macros`; terminal menu links to it
- **v4** — baseline import, locked. Available as git tag `v4`
  (GitHub → Tags → `v4` → download ZIP, or `git checkout v4`).
