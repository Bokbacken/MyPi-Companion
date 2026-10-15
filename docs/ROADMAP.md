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
- Pi-side sources in [`pi/`](../pi/) incl. `uninstall.sh`; `tools/gen_rigsetup_h.py`
- GitHub Actions build (`arduino-cli`) with freshness checks for generated headers

---

## v6 (in progress, branch per feature)

### v6.1 CLI / headless install — implemented, hardware test pending
- Install mode Desktop / CLI chosen on `/installonpi` (stored in NVS `rigcfg`, except the
  password, which is RAM-only and wiped after each run)
- CLI: console login on tty1 via HID, `sudo` via `read -rs` + `sudo -S -v`, mount
  `RIGSETUP` by label, install, unmount, log out
- Keyboard layout selectable (Auto / US / Swedish-Danish / German / French / ES-IT-PT)
- `install.sh`: unzip the offline pyserial wheel when pip is missing (Lite), service gets
  `PYTHONPATH=/opt/rig-console/lib`; new code `RIG:INSTALL_ERR_DEPS`
- USB networking (NCM) is deprioritized: with CLI install it is not needed

### v6.1 multi-session terminal — hardware-tested
- Up to 4 independent PTY sessions (one per browser tab), slot-tagged serial protocol
- Shared command history across tabs (typed commands only), slot number in saved-session filename

### v6.1 AP profiles + NAT — hardware-tested
- 4 editable AP profiles (name, SSID, password, subnet preset, NAT) — touch picker + `/ap-profiles`
- NAT internet sharing via ESP32 NAPT (~6 Mbit/s), forced client DNS 1.1.1.1, `/api/apstatus`

### v6.3 Pi system info + ESP web connect — hardware-tested (AP-aware STA soak test passed: ~10 h, 2026-10-15)
- Daemon sends `STATUS:SYS:key=value|…` (hostname, model, OS, kernel, daemon version, uptime,
  load, CPU temp, RAM, disk, throttle flags, Wi-Fi dBm, NTP). Key/value format: new fields
  can be added without a protocol change; unknown keys are shown raw on `/info`
- System Info on the touch screen has two pages (ESP / Pi); `/info` has a Raspberry Pi card
- `/profiles` can connect the **ESP itself** (scan, SSID + password, or a saved slot);
  previously only the Pi could be connected from the web
- Non-blocking, AP-aware ESP STA connect: joining a network no longer kills a running AP
  (AP on boot), stops the web server or freezes the UI
- AP client host names (Pi / DHCP option 12 / mDNS / NetBIOS), toast channel for all action
  feedback, faster OTA (5 % display steps) with a web *Flashing* phase
- v6.3.3: explicit AP client limit of 8 (`AP_MAX_CLIENTS`; DHCP server limit), shown as `n/8`

### Later v6 candidates
RPC channel + daemon versioning · web login / PIN · robustness (connection limit, OTA
rollback) · Pi system panel · Pi file manager · recipes + placeholders · web UI cleanup.

### Planned: AP without 802.11b (next firmware batch)
Always better: 802.11b rates (1–11 Mbit/s DSSS) take a lot of airtime per frame.
Beacons and management frames are also sent at 1 Mbit/s while 11b is on. No current
client needs 11b (S25, Tab S10, Pi 5, laptops).
- AP interface only g/n: `esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11G |
  WIFI_PROTOCOL_11N)` after `WiFi.softAP()`; to verify on core 3.3.x / IDF 5.x:
  `esp_wifi_config_11b_rate(WIFI_IF_AP, true)` so beacons go out at 6 Mbit/s OFDM
- STA uplink unchanged (it must be able to join b/g/n networks such as hotel/office APs)
- Show the active AP protocol on the `Radio` line of the AP diagnostics (e.g. `AP g/n`)
- Hardware check: Wi-Fi Scanner shows the RigSetup mode as `g, n` (today `b, g, n`),
  and all clients still connect
- Background: Delft office 2026-10-15, uplink on ch 1 next to overlapping ch 4 networks
  (Procentec) gave 0.19–1.45 Mbit/s; the AP has to share channel and airtime with the
  uplink, so every frame we save counts

---

## Next (priority order)

### 1. Placeholders with remembered values — high (design confirmed)
- **Syntax:** `<NAME.>` / `<NAME:>` / `<NAME*>` where the suffix defines type and save
  behaviour. `NAME`: uppercase letters, digits and `_`, matching `[A-Z][A-Z0-9_]*`. Shell
  redirection (`cat < file`, `<<EOF`) does not match. Optional default value:
  `<HOST=192.168.4.1.>` (default before the suffix). No suffix = treated as `.`
  (backward-compatible with existing `examples/`).
  - `.` — single value, auto-saved to FFat on run; popup prefills from saved value.
  - `:` — list, remembered as an ordered list (most recent first, up to 25 items),
    auto-saved on run; popup shows a dropdown of the recent list.
  - `*` — entered each time, never saved; popup shows a blank masked field with 👁 toggle.
    Use for one-off passwords and tokens.
- **Consistent type per name:** when the same NAME appears in multiple macros the suffix
  must match. The management UI enforces this and offers to auto-update all occurrences
  when the type is changed.
- **Combined tokens:** a compound token like `<IP/PREFIX>` should be split into separate
  typed placeholders, e.g. `<IP:>/<PREFIX.>`. This is already reflected in `examples/`.
- **Index:** built automatically by scanning the macro pool. There is no separate list to
  maintain. A placeholder only gets a stored entry once it has a value or settings.
- **Run flow:** clicking a macro with placeholders opens a popup.
  - One field per unique placeholder, prefilled with the saved value (or, for `:`, a
    dropdown of the recent list).
  - Live preview of the final command line.
  - Enter runs the macro, Esc cancels.
  - Shift+click (or long-press) runs the macro immediately when every placeholder already
    has a non-empty value.
- **Hidden / sensitive values:** `*` fields are always masked with a 👁 toggle. `.` and `:`
  fields whose NAME contains PASS, PSK, TOKEN, SECRET or KEY are masked by default; this
  can be overridden per placeholder. FFat is not encrypted — the same level as today's
  macro and Wi-Fi password storage. A value is still echoed by the shell once the command
  runs.
- **Macro names:** `[TEMPLATE]` suffixes are removed from macro names. Instead, any macro
  whose `cmd` contains a placeholder shows a small indicator badge in the macro list —
  visible at a glance without cluttering the name.
- **Placeholders page:** a 4th tab in `/macros` and *Placeholders...* in the terminal menu.
  - Columns: name, type (`.` / `:` / `*`), used in N macros, value / list, hidden flag.
  - Actions: set a value or add to a list in advance, clear one entry, *Clear all*.
- **Storage:** `/config/placeholders.json` on FFat. Limits: list max 25 items, value max 64
  characters. API `/api/ph` (GET/POST/DELETE).

### 2. File menu — save session — medium
**Partly done:** *Save session…* saves the output as txt, and scrollback is already 10 000
lines. Remaining: html (keeps ANSI colours) and md formats, or input history only.

### 3. Command history — medium
**Partly done:** shared history (last 100, `localStorage`), Ctrl+Up/Down; macro commands are
not stored. Remaining: Ctrl+R search, a history view (click runs the command, Shift+click
*Add as macro*), export as txt/md.

### 4. Web file editor — medium
`/editor`, CodeMirror 6 (~80 KB gzip) or a minimal custom editor. Daemon reads the file →
base64 over serial → browser; save goes back and the daemon writes with `sudo tee`.
**Requires daemon changes** (now possible: edit `pi/rig_daemon.py`, regenerate
`rigsetup_files.h`, reinstall on the Pi).

### 5. File upload / download — medium
Drag-and-drop upload, chunked base64 over WebSocket; download via daemon.
USB CDC ≈ 11 KB/s net — fine for config files, not for large binaries.
**Requires daemon changes.**

### 6. Service manager — low
`/services`: systemd units with status, start/stop/restart, journalctl output.

### 7. System monitor widget — low
**Partly done in v6.3:** `STATUS:SYS` delivers temp, RAM, disk, load (shown on System Info
and `/info`). Remaining: CPU % and a compact widget on the dashboard / status bar.

---

## Older open items

- Headless Pi install (Ctrl+Alt+T does not work without a desktop)
- `CMD:ADDWIFI_FULL` — handle the Pi's confirmation (currently ignored)
- QR code with `http://rigconsole.local` on the display
- ~~Extended Pi health (CPU temp, uptime) on `STATE_ADV_INFO`~~ — done in v6.3 (Pi page)
- NVS profile export/import (`/api/nvsimport`)
- Optional: put `uninstall.sh` on the RIGSETUP disk too

## Longer term

- SSH proxy via USB NCM (S3 as USB Ethernet)
- Scheduled Wi-Fi scans reported by the daemon
- Connection event log on FFat (ring buffer)
- Multiple Pi devices — select the active Pi
- Touch PIN for authentication during install
- **Dual-band hardware: ESP32-P4 + ESP32-C5** (ordered). The S3 only has 2.4 GHz and one
  radio, so the AP must share channel and airtime with the uplink. The C5 adds 5 GHz but
  lacks the S3's USB device roles; the P4 provides USB (CDC, HID, MSC) and the C5 acts as
  the Wi-Fi co-processor. New hardware port, not a firmware upgrade of the S3 build

---

## Storage notes

The FATFS partition gives ~12.5 MB — ample for text: a 10 000-line session ≈ 1.2 MB,
200 macros ≈ 40 KB. An SD card is only relevant for OTA images for many Pis, long-term
log archiving, large binaries over MSC, or measurement data.
