# Pi-side files

This folder contains the source files that are embedded in the ESP32 firmware
as a virtual USB disk (`msc_image.h`).  When the user runs **Install on Pi**
from the Rig Console web UI or touch menu, the ESP32 mounts this disk and the
Pi runs `install.sh` via HID automation.

## Files

| File | Purpose |
|------|---------|
| `install.sh` | Installer — copies daemon, installs pyserial, registers systemd service |
| `uninstall.sh` | Uninstaller — stops services, removes daemon and unit files |
| `rig_daemon.py` | Serial daemon that runs on the Pi (`/opt/rig-console/rig_daemon.py`) |
| `rig-console.service` | Reference copy of the systemd unit (written inline by install.sh) |
| `pyserial-3.5-py2.py3-none-any.whl` | Bundled offline wheel — no internet needed during install |

## Regenerating msc_image.h

After editing any file in this folder, regenerate the embedded disk image:

```bash
python3 tools/gen_msc_image.py
```

This updates `HMS_RigConsole_v5/msc_image.h`.  Commit both the source files
and the regenerated header together.

## Notes

- `uninstall.sh` is **not** placed on the MSC disk (it would need to be run
  manually anyway).  It lives here for easy download and reference.
- `rig-terminal.service` (ttyd) is not used in v5; the web terminal runs over
  the serial protocol (`TERM:IN` / `TERM:OUT`) instead.
