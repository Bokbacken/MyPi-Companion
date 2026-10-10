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
└── README.md
```

The sketch folder name must match the `.ino` file name (Arduino requirement).

## Build & flash (Arduino IDE)

1. Open `HMS_RigConsole_v5/HMS_RigConsole_v5.ino`
2. Board: **ESP32S3 Dev Module**
3. Partition scheme: **16M Flash (2MB APP/12.5MB FATFS)**
4. Flash mode: **QIO 80MHz**
5. Compile & upload — or upload the `.bin` via the web page `/update` (OTA)

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
- **v4** — baseline import
