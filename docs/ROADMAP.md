# MyPi-Companion — Roadmap

_Last updated: October 2026_

## Done

### v4 (tag `v4`)
ESP32-S3 USB OTG · AXS5106L touch UI (swipe + tap) · TinyUSB CDC + HID + MSC ·
Pi install via HID keyboard · MSC exposes `rig_daemon.py` · 10 NVS slots with static IP ·
`/profiles` manager · web server in STA mode + mDNS `rigconsole.local` · web OTA (`/update`) ·
xterm.js terminal over WebSocket · ESP profile selection without AP · redesigned dashboard ·
Pi Wi-Fi scan (`CMD:SCAN_WIFI`) · PSRAM framebuffer · 25 Quick Buttons (NVS, vertical list) ·
macro pool on FFat · base64-correct macros · `termFit()` · FFat file management via web.

### v5
- GUI macro & menu editor at `/macros` (library, drag/click into menus and submenus,
  separators, QB slots, JSON import/export)
- Terminal "Edit macros & menus…" opens `/macros`
- 58 example Raspberry Pi macros in [`examples/`](../examples/)
- Web OTA fix (multipart), FFat fix (removed `partitions.csv`)
- HID keyboard layout auto-detection; auto-connect after install
- `STATUS:CONN`: real SSID on the display, NM connection name for switching
- Pi-side sources in [`pi/`](../pi/) incl. `uninstall.sh`; `tools/gen_msc_image.py`
- GitHub Actions build (`arduino-cli`) with freshness checks for generated headers

---

## Next (priority order)

### 1. Placeholders with session memory — high
Macros may contain placeholders (the examples use `<NAME>`, the original plan used `{name}`
— pick one syntax). Values live in browser `sessionStorage`, never stored on the ESP32.
A popup opens when a macro contains placeholders; per-placeholder `hide` flag (masked
input with 👁 toggle); "Clear" resets session values.

### 2. File menu — save session — medium
Save all output as txt / html (keeps ANSI colors) / md, or input history only.
Iterate `term.buffer.active` in the browser; download via `<a download>`; raise scrollback
to 10 000 lines (currently 5 000).

### 3. Command history — medium
Capture input via `term.onData`. Ctrl+R searchable history (last 100). History view:
click to run, Shift+click to "Add as macro", export txt/md.

### 4. Web file editor — medium
`/editor`, CodeMirror 6 (~80 KB gzip) or a minimal custom editor. Daemon reads the file →
base64 over serial → browser; save goes back and the daemon writes with `sudo tee`.
**Requires daemon changes** (now possible: edit `pi/rig_daemon.py`, regenerate
`msc_image.h`, reinstall on the Pi).

### 5. File upload / download — medium
Drag-and-drop upload, chunked base64 over WebSocket; download via daemon.
USB CDC ≈ 11 KB/s net — fine for config files, not for large binaries.
**Requires daemon changes.**

### 6. Service manager — low
`/services`: systemd units with status, start/stop/restart, journalctl output.

### 7. System monitor widget — low
`CPU: 23%  RAM: 312/1024 MB  Temp: 48°C  Disk: 12.3/32 GB`, polled every 10 s.
New message `STATUS:SYS:cpu:ram:temp:disk` (**daemon change**).

---

## Older open items

- Headless Pi install (Ctrl+Alt+T does not work without a desktop)
- `CMD:ADDWIFI_FULL` — handle the Pi's confirmation (currently ignored)
- QR code with `http://rigconsole.local` on the display
- Extended Pi health (CPU temp, uptime) on `STATE_ADV_INFO`
- NVS profile export/import (`/api/nvsimport`)
- Optional: put `uninstall.sh` on the RIGSETUP disk too

## Longer term

- SSH proxy via USB NCM (S3 as USB Ethernet)
- Scheduled Wi-Fi scans reported by the daemon
- Connection event log on FFat (ring buffer)
- Multiple Pi devices — select the active Pi
- Touch PIN for authentication during install

---

## Storage notes

The FATFS partition gives ~12.5 MB — ample for text: a 10 000-line session ≈ 1.2 MB,
200 macros ≈ 40 KB. An SD card is only relevant for OTA images for many Pis, long-term
log archiving, large binaries over MSC, or measurement data.
