# MyPi-Companion — Working Guidelines

_Agreements and preferences for this project._

## Language

- Discussion may be in Swedish; **everything committed is US English**: code, comments,
  UI text, protocol messages, macro names, documentation.

## Approach

- Correctness over speed. Do not guess — if something is unclear, ask.
- Read the affected code before editing; never work from assumptions.
- Smallest change that solves the problem — no restructuring without a reason.
- Verify brace balance and symbol references after every edit.
- Edit in logical order: `config.h` → globals → protocol → display → touch.

## Version control

- Repository: `Bokbacken/MyPi-Companion` (private).
- `main` is the stable branch. All changes go on a feature branch
  (`feature/...`, `fix/...`, `docs/...`) and are merged via a reviewed pull request.
- v4 is locked and preserved as tag `v4`. Major versions are tagged (`v5`, `v6`, ...).
- A new major version: rename the sketch folder and `.ino` (`git mv`) and update
  version strings in `config.h`, the `.ino` header and the splash text.
- Never commit build output (`*.bin`, `*.zip`) — see `.gitignore`.
- Documentation is Markdown (`.md`) only — no PDF/Word copies.

### Generated files (never edit by hand)
- `terminal_html.h` ← `HMS_RigConsole_v6/gen_terminal_html.py`
- `rigsetup_files.h` ← `pi/` via `tools/gen_rigsetup_h.py`
- Commit the source and the generated file together; CI fails if they differ.

## Hard technical rules

### Hardware
- ESP32-S3, USB OTG, OPI PSRAM, 16 MB flash; 172×320 portrait display.
- Touch tap only registers in the bottom 20 px (y ≥ 300) — intentional.
- No physical buttons. USB is TinyUSB composite (CDC + HID + MSC), not a USB-UART bridge.

### Communication
- Serial 115 200 baud, newline-terminated ASCII.
- Pi installation always via USB (HID + MSC) — never via AP/network.
- The AP is for laptop configuration (web UI) only.
- Pi-side sources live in `pi/`. Change `rig_daemon.py` / `install.sh` only when the
  feature requires it, keep the serial protocol backward compatible where possible, and
  always regenerate `rigsetup_files.h` with `python3 tools/gen_rigsetup_h.py` (CI checks it).

### Web server
- Starts in STA mode as well as AP mode.
- Always stop with `webServerStop()` — never `server.stop()` directly.

### Terminal
- `gen_terminal_html.py` is the **only** source for `terminal_html.h` —
  edit the `.py`, run `python3 gen_terminal_html.py`, commit both files.
- No FitAddon — use `termFit()`.
- Quick-button panel is always a vertical list (`flex-direction:column`).

### Storage
- FFat (not LittleFS, despite `littlefs_mgr.ino`).
- Partition scheme set in Arduino IDE: "16M Flash (2MB APP/12.5MB FATFS)" — no custom CSV.
- QB slot ids in NVS (`"qbtns"`); `macros.json` / `menus.json` on FFat, client-side only.

### Wi-Fi
- No auto-AP; AP is started manually from the Advanced menu.
- 10 shared NVS slots tried in order 0–9.

### Build
- `Serial.setRxBufferSize(4096)` before `Serial.begin()`.
- `.ino` tabs are concatenated alphabetically — no `extern` needed.

### Splash
- White background — use `drawProgmemImageTransp(..., 0x0000)` for logos.
