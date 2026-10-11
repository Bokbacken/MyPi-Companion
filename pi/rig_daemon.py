#!/usr/bin/env python3
"""
HMS Rig Console — Raspberry Pi daemon  (rig_daemon.py)
=======================================================
Handles serial communication with the ESP32 front panel.

Protocol  (115200 baud, newline-terminated ASCII)
-------------------------------------------------
ESP32 → Pi:
  HELLO                        — sent at startup / reconnect
  CMD:GET_STATUS               — poll current SSID + IP (every 5 s after first ACK)
  CMD:CONNECT:<profile>        — user selected a network in the menu
  CMD:ADDWIFI:<s>:<pw>         — user added a new WiFi network via web UI
  CMD:ADDWIFI_FULL:<s>|<pw>|<dhcp>|<ip>|<sn>|<gw>|<dns>
                               — full NVS-slot profile push from web UI
  CMD:GET_PROFILES_FULL        — request profile list with passwords
  CMD:SCAN_WIFI                — trigger nmcli Wi-Fi scan; Pi replies with
                                 SCAN_RESULT:ssid:signal:freq per network,
                                 then SCAN_DONE when finished
  CMD:START_AP                 — user started AP mode (informational)
  CMD:STOP_AP                  — user closed AP mode (informational)
  CMD:TERM_OPEN                — open PTY bash terminal session
  CMD:TERM_CLOSE               — close PTY bash terminal session
  CMD:MSC_EJECT                — ESP is about to remove its USB disk (SD Share /
                                 RIGSETUP): sync + unmount it, then reply
  TERM:IN:<b64>                — base64-encoded keystrokes from browser terminal
  CTL:RESIZE:<cols>:<rows>     — browser terminal resize notification

Pi → ESP32:
  ACK                          — response to HELLO
  PONG                         — response to PING
  PROFILES:<p1>,<p2>           — comma-separated list of Wi-Fi profile names
  PROFILES_FULL:<s1>|<pw1>,…   — profile names with passwords
  STATUS:SSID:<ssid>           — current Wi-Fi SSID (or NONE)
  STATUS:CONN:<name>           — current NetworkManager connection name (or NONE)
  STATUS:IP:<ip>               — current IP (or NONE)
  STATUS:PORT:<device>         — USB serial port in use (e.g. /dev/ttyACM0)
  OK:CONNECT:<profile>         — connection succeeded
  ERR:CONNECT:<profile>        — connection failed
  OK:ADDWIFI:<ssid>            — profile added successfully
  ERR:ADDWIFI:<ssid>           — profile could not be added
  OK:ADDWIFI_FULL:<ssid>       — full profile added and connected
  ERR:ADDWIFI_FULL:<ssid>      — full profile failed
  SCAN_RESULT:<ssid>:<signal>:<freq>  — one network found during scan
  SCAN_DONE                    — scan finished (or failed silently)
  TERM:OUT:<b64>               — base64-encoded PTY output to browser terminal
  OK:MSC_EJECT                 — ESP disk flushed and unmounted (safe to remove)
  ERR:MSC_EJECT:<reason>       — flushed, but unmount failed

Requirements: python3-serial (apt-get install python3-serial)
Permissions:  user in group 'dialout', polkit rule for nmcli
"""

import base64
import glob
import json
import logging
import os
import pty
import select
import signal
import subprocess
import sys
import threading
import time
from typing import List, Optional

import serial  # apt-get install python3-serial

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
BAUD              = 115_200
POLL_INTERVAL     = 2.0   # seconds between port scans
KEEPALIVE_SECS    = 30    # send STATUS:* at this interval
WIFI_IFACE        = "wlan0"
POST_CONNECT_WAIT = 3     # seconds to wait after 'nmcli conn up'

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
    handlers=[logging.StreamHandler(sys.stdout)],
)
log = logging.getLogger(__name__)

# Protects all serial writes (main thread + PTY reader thread)
_serial_lock = threading.Lock()

# USB port currently in use (set in main() around the serial context)
_active_port: str = ""


# ---------------------------------------------------------------------------
# Find ESP32 port
# ---------------------------------------------------------------------------
def find_esp32_port() -> Optional[str]:
    """Return the first available ACM/USB serial port, or None."""
    for pattern in ("/dev/ttyACM*", "/dev/ttyUSB*"):
        ports = sorted(glob.glob(pattern))
        if ports:
            return ports[0]
    return None


# ---------------------------------------------------------------------------
# nmcli helpers
# ---------------------------------------------------------------------------
def _nmcli(*args: str, timeout: int = 10) -> str:
    try:
        r = subprocess.run(
            ["nmcli", "-t", *args],
            capture_output=True, text=True, timeout=timeout,
        )
        return r.stdout.strip()
    except Exception as exc:
        log.error("nmcli error: %s", exc)
        return ""


def get_wifi_profiles() -> List[str]:
    """List configured WiFi profiles from NetworkManager."""
    out = _nmcli("-f", "NAME,TYPE", "connection", "show")
    profiles: List[str] = []
    for line in out.splitlines():
        parts = line.split(":", 1)
        if len(parts) == 2 and "wireless" in parts[1].lower():
            name = parts[0].strip()
            if name:
                profiles.append(name)
    return profiles


def get_current_ssid() -> str:
    """Return the active SSID on wlan0, or NONE.

    Uses 'nmcli -t -f ACTIVE,SSID dev wifi' which reports the actual
    wireless SSID — not the NetworkManager connection name (GENERAL.CONNECTION),
    which can be an auto-generated string like 'netplan-wlan0-TOFTA'.
    """
    out = _nmcli("-f", "ACTIVE,SSID", "dev", "wifi")
    for line in out.splitlines():
        # Output format: "yes:<ssid>" for the active network.
        # SSID may contain colons; split on first colon only.
        parts = line.split(":", 1)
        if len(parts) == 2 and parts[0].strip() == "yes":
            ssid = parts[1].strip()
            return ssid if ssid else "NONE"
    return "NONE"


def get_current_conn() -> str:
    """Return the active NetworkManager connection name on wlan0, or NONE.

    Uses 'nmcli -t -f GENERAL.CONNECTION device show wlan0' which reports
    the NM connection name (e.g. 'Tofta' or 'netplan-wlan0-TOFTA') — used
    by CMD:CONNECT to activate a profile by name.
    """
    out = _nmcli("-f", "GENERAL.CONNECTION", "device", "show", WIFI_IFACE)
    for line in out.splitlines():
        if "GENERAL.CONNECTION" in line:
            name = line.split(":", 1)[-1].strip()
            return name if name and name != "--" else "NONE"
    return "NONE"


def get_current_ip() -> str:
    """Return the current IPv4 address on wlan0, or NONE."""
    out = _nmcli("-f", "IP4.ADDRESS", "device", "show", WIFI_IFACE)
    for line in out.splitlines():
        if "IP4.ADDRESS" in line:
            addr = line.split(":", 1)[-1].strip().split("/")[0]
            return addr if addr and addr != "--" else "NONE"
    return "NONE"


def connect_profile(profile: str) -> bool:
    """Activate a WiFi profile via nmcli. Returns True on success."""
    log.info("Connecting to: %s", profile)
    try:
        r = subprocess.run(
            ["nmcli", "connection", "up", profile],
            capture_output=True, text=True, timeout=30,
        )
        if r.returncode != 0:
            log.warning("nmcli output: %s", r.stderr.strip() or r.stdout.strip())
        return r.returncode == 0
    except subprocess.TimeoutExpired:
        log.error("Timeout connecting to %s", profile)
        return False
    except Exception as exc:
        log.error("Connection error: %s", exc)
        return False


def subnet_to_prefix(sn: str) -> str:
    """Convert dotted-decimal subnet mask OR plain CIDR integer to prefix length."""
    import ipaddress
    sn = sn.strip()
    try:
        n = int(sn)
        if 0 <= n <= 32:
            return str(n)
    except ValueError:
        pass
    try:
        net = ipaddress.IPv4Network(f"0.0.0.0/{sn}", strict=False)
        return str(net.prefixlen)
    except Exception:
        return "24"


def get_profiles_full() -> List[tuple]:
    """Return list of (ssid, psk) for all NM WiFi connections (polkit grants PSK read)."""
    names = get_wifi_profiles()
    result: List[tuple] = []
    for name in names:
        try:
            r = subprocess.run(
                ["nmcli", "-s", "-t", "-f",
                 "802-11-wireless.ssid,802-11-wireless-security.psk",
                 "connection", "show", name],
                capture_output=True, text=True, timeout=10,
            )
            ssid = ""
            psk  = ""
            for line in r.stdout.splitlines():
                if line.startswith("802-11-wireless.ssid:"):
                    ssid = line.split(":", 1)[1].strip()
                elif line.startswith("802-11-wireless-security.psk:"):
                    psk = line.split(":", 1)[1].strip()
            if not ssid:
                ssid = name
            result.append((ssid, psk))
        except Exception as exc:
            log.error("get_profiles_full error for %s: %s", name, exc)
            result.append((name, ""))
    return result


def add_wifi_full(ssid: str, passwd: str, dhcp: bool,
                  ip: str, sn: str, gw: str, dns: str) -> bool:
    """Add or modify a WiFi connection with optional static IP, then bring it up."""
    log.info("add_wifi_full: ssid=%r dhcp=%s ip=%s", ssid, dhcp, ip)
    existing = get_wifi_profiles()

    if ssid in existing:
        cmd = ["nmcli", "connection", "modify", ssid]
    else:
        cmd = [
            "nmcli", "connection", "add",
            "type",     "wifi",
            "con-name", ssid,
            "ifname",   WIFI_IFACE,
            "ssid",     ssid,
        ]

    if passwd:
        cmd += ["wifi-sec.key-mgmt", "wpa-psk", "wifi-sec.psk", passwd]

    if dhcp:
        cmd += ["ipv4.method", "auto"]
        if ssid in existing:
            cmd += ["ipv4.addresses", "", "ipv4.gateway", "", "ipv4.dns", ""]
    else:
        prefix = subnet_to_prefix(sn)
        cmd += ["ipv4.method", "manual",
                "ipv4.addresses", f"{ip}/{prefix}"]
        if gw:
            cmd += ["ipv4.gateway", gw]
        if dns:
            cmd += ["ipv4.dns", dns]

    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
        if r.returncode != 0:
            log.warning("nmcli add/modify error: %s",
                        r.stderr.strip() or r.stdout.strip())
            return False
    except Exception as exc:
        log.error("add_wifi_full error: %s", exc)
        return False

    return connect_profile(ssid)


def add_wifi_profile(ssid: str, password: str) -> bool:
    """Add a new WiFi network to NetworkManager via nmcli."""
    log.info("Adding WiFi profile: '%s' (password: %s)", ssid,
             "yes" if password else "no/open")
    try:
        cmd = [
            "nmcli", "connection", "add",
            "type",     "wifi",
            "con-name", ssid,
            "ifname",   WIFI_IFACE,
            "ssid",     ssid,
        ]
        if password:
            cmd += ["wifi-sec.key-mgmt", "wpa-psk", "wifi-sec.psk", password]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
        if r.returncode != 0:
            log.warning("nmcli add error: %s", r.stderr.strip() or r.stdout.strip())
        return r.returncode == 0
    except subprocess.TimeoutExpired:
        log.error("Timeout adding profile: %s", ssid)
        return False
    except Exception as exc:
        log.error("Error adding profile: %s", exc)
        return False


def scan_wifi(ser: serial.Serial) -> None:
    """
    Trigger a Wi-Fi scan via nmcli and send results line by line.

    Each result: SCAN_RESULT:<ssid>:<signal>:<freq>
    Finished:    SCAN_DONE
    The SSID field may contain colons (rare) but signal and freq never do,
    so we send them colon-separated and let the ESP split from the right.
    Empty SSIDs (hidden networks) are skipped.
    """
    log.info("Scanning Wi-Fi networks...")
    try:
        r = subprocess.run(
            ["nmcli", "-t", "-f", "SSID,SIGNAL,FREQ",
             "dev", "wifi", "list", "--rescan", "yes"],
            capture_output=True, text=True, timeout=20,
        )
        seen = set()
        for line in r.stdout.strip().splitlines():
            # nmcli -t separates fields with ':'
            # SSID itself may contain colons so split from the right
            parts = line.rsplit(":", 2)
            if len(parts) < 3:
                continue
            ssid   = parts[0].strip()
            signal = parts[1].strip()
            freq   = parts[2].strip()
            if not ssid or ssid in seen:
                continue
            seen.add(ssid)
            send(ser, f"SCAN_RESULT:{ssid}:{signal}:{freq}")
            time.sleep(0.02)   # avoid flooding the serial buffer
        log.info("Scan complete: %d networks found", len(seen))
    except subprocess.TimeoutExpired:
        log.error("Wi-Fi scan timed out")
    except Exception as exc:
        log.error("Scan error: %s", exc)
    finally:
        send(ser, "SCAN_DONE")


# ---------------------------------------------------------------------------
# ESP32 USB disk (MSC) eject
# ---------------------------------------------------------------------------
ESP_MSC_VENDOR = "HMS"   # SCSI vendor ID reported by msc.ino


def _esp_msc_mounts() -> List[tuple]:
    """Return [(device, mountpoint)] for mounted partitions of the ESP32 disk."""
    r = subprocess.run(
        ["lsblk", "-J", "-p", "-o", "NAME,VENDOR,MOUNTPOINT"],
        capture_output=True, text=True, timeout=10,
    )
    mounts = []

    def walk(node: dict) -> None:
        if node.get("mountpoint"):
            mounts.append((node["name"], node["mountpoint"]))
        for child in node.get("children") or []:
            walk(child)

    for disk in json.loads(r.stdout or "{}").get("blockdevices", []):
        if (disk.get("vendor") or "").strip() == ESP_MSC_VENDOR:
            walk(disk)
    return mounts


def _unmount(dev: str, mnt: str, lazy: bool) -> subprocess.CompletedProcess:
    """udisksctl first (works without root for udisks mounts), then sudo umount."""
    cmd = ["udisksctl", "unmount", "-b", dev, "--no-user-interaction"]
    if lazy:
        cmd.append("--force")              # udisks: --force = lazy unmount
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
    if r.returncode != 0:
        # Fallback for mounts not owned by udisks (e.g. manual sudo mount)
        cmd = ["sudo", "-n", "umount"] + (["-l"] if lazy else []) + [mnt]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
    return r


def _holders(mnt: str) -> str:
    """Names of processes keeping mnt busy (best effort, for the log)."""
    try:
        r = subprocess.run(["fuser", "-vm", mnt], capture_output=True, text=True, timeout=5)
        names = {ln.split()[-1] for ln in r.stderr.splitlines()[1:] if ln.split()}
        names.discard(mnt + ":")
        return ", ".join(sorted(names)) or "unknown"
    except Exception:
        return "unknown"


def eject_esp_msc() -> Optional[str]:
    """
    Flush and unmount the ESP32 USB disk before the ESP removes it.
    Returns None on success, otherwise a short error string.
    sync always runs, so cached writes are on the card even if unmount fails.
    """
    try:
        subprocess.run(["sync"], timeout=30)
        errors = []
        for dev, mnt in _esp_msc_mounts():
            r = _unmount(dev, mnt, lazy=False)
            if r.returncode != 0 and "busy" in (r.stderr or "").lower():
                # Something (typically a file manager window) still has the
                # mount open.  Data is already synced and the ESP removes the
                # media regardless, so detach lazily instead of leaving a
                # dead mount behind.
                log.warning("%s busy (held by: %s) — lazy unmount", mnt, _holders(mnt))
                r = _unmount(dev, mnt, lazy=True)
            if r.returncode == 0:
                log.info("Unmounted %s (%s)", dev, mnt)
            else:
                errors.append(f"{dev}: {(r.stderr or '').strip()[:60]}")
        return "; ".join(errors) or None
    except Exception as exc:
        return str(exc)[:80]


# ---------------------------------------------------------------------------
# Sending  (always hold _serial_lock)
# ---------------------------------------------------------------------------
def send(ser: serial.Serial, msg: str) -> None:
    with _serial_lock:
        try:
            ser.write((msg + "\n").encode("utf-8"))
            log.info("→ %s", msg if len(msg) < 80 else msg[:60] + "…")
        except (serial.SerialException, OSError) as exc:
            log.warning("Send error: %s", exc)
            raise serial.SerialException(str(exc))


def send_status(ser: serial.Serial) -> None:
    ssid = get_current_ssid()
    conn = get_current_conn()
    ip   = get_current_ip()
    send(ser, f"STATUS:SSID:{ssid}")
    time.sleep(0.05)
    send(ser, f"STATUS:CONN:{conn}")
    time.sleep(0.05)
    send(ser, f"STATUS:IP:{ip}")
    time.sleep(0.05)
    send(ser, f"STATUS:PORT:{_active_port}")


def send_profiles(ser: serial.Serial) -> None:
    profiles = get_wifi_profiles()
    payload  = ",".join(profiles) if profiles else "NONE"
    send(ser, f"PROFILES:{payload}")
    log.info("Profiles: %s", profiles)


# ---------------------------------------------------------------------------
# PTY  — bash terminal bridged over USB serial
# ---------------------------------------------------------------------------
_term_master: Optional[int]      = None
_term_pid:    Optional[int]      = None
_term_stop                       = threading.Event()


def _term_reader(master_fd: int, ser: serial.Serial) -> None:
    """Read PTY output, base64-encode it, and send to ESP32 over serial."""
    while not _term_stop.is_set():
        try:
            r, _, _ = select.select([master_fd], [], [], 0.1)
            if not r:
                continue
            data = os.read(master_fd, 4096)
            if not data:
                break
            b64 = base64.b64encode(data).decode("ascii")
            with _serial_lock:
                ser.write(("TERM:OUT:" + b64 + "\n").encode("utf-8"))
        except OSError:
            break
    log.info("PTY reader thread exiting.")


def open_terminal(ser: serial.Serial) -> None:
    """Fork a bash child attached to a PTY and start the reader thread."""
    global _term_master, _term_pid
    close_terminal()
    try:
        pid, master_fd = pty.fork()
    except OSError as exc:
        log.error("pty.fork() failed: %s", exc)
        return
    if pid == 0:
        os.environ["TERM"] = "xterm-256color"
        os.execvp("bash", ["bash", "-l"])
        os._exit(1)
    _term_pid    = pid
    _term_master = master_fd
    _term_stop.clear()
    t = threading.Thread(target=_term_reader, args=(master_fd, ser), daemon=True)
    t.start()
    log.info("Terminal opened (PID %d)", pid)


def close_terminal() -> None:
    """Kill bash and close the PTY."""
    global _term_master, _term_pid
    _term_stop.set()
    if _term_pid is not None:
        try:
            os.kill(_term_pid, signal.SIGTERM)
            os.waitpid(_term_pid, os.WNOHANG)
        except OSError:
            pass
        _term_pid = None
    if _term_master is not None:
        try:
            os.close(_term_master)
        except OSError:
            pass
        _term_master = None
    log.info("Terminal closed.")


def term_input(b64_data: str) -> None:
    """Decode base64 keystrokes from browser and write them to the PTY."""
    if _term_master is None:
        return
    try:
        data = base64.b64decode(b64_data)
        os.write(_term_master, data)
    except Exception as exc:
        log.error("PTY write error: %s", exc)


def term_resize(cols: int, rows: int) -> None:
    """Tell the PTY (and bash/nano/etc.) about a new terminal size."""
    if _term_master is None:
        return
    try:
        import fcntl, struct, termios
        winsize = struct.pack("HHHH", rows, cols, 0, 0)
        fcntl.ioctl(_term_master, termios.TIOCSWINSZ, winsize)
    except Exception as exc:
        log.warning("PTY resize error: %s", exc)


# ---------------------------------------------------------------------------
# Message handling
# ---------------------------------------------------------------------------
def dispatch(ser: serial.Serial, msg: str) -> None:
    """Parse a message from ESP32 and respond."""
    log.info("← %s", msg)

    if msg == "HELLO":
        send(ser, "ACK")
        time.sleep(0.10)
        send_profiles(ser)
        time.sleep(0.05)
        send_status(ser)

    elif msg == "PING":
        send(ser, "PONG")

    elif msg == "CMD:GET_STATUS":
        send_status(ser)

    elif msg.startswith("CMD:CONNECT:"):
        profile = msg[len("CMD:CONNECT:"):]
        ok = connect_profile(profile)
        time.sleep(POST_CONNECT_WAIT)
        if ok:
            send_status(ser)
            send(ser, f"OK:CONNECT:{profile}")
        else:
            send(ser, f"ERR:CONNECT:{profile}")

    elif msg.startswith("CMD:ADDWIFI:"):
        # Format: CMD:ADDWIFI:<ssid>:<password>
        rest   = msg[len("CMD:ADDWIFI:"):]
        sep    = rest.find(":")
        ssid   = rest[:sep] if sep >= 0 else rest
        passwd = rest[sep + 1:] if sep >= 0 else ""
        ok = add_wifi_profile(ssid, passwd)
        if ok:
            time.sleep(1)
            send_profiles(ser)
            send(ser, f"OK:ADDWIFI:{ssid}")
        else:
            send(ser, f"ERR:ADDWIFI:{ssid}")

    elif msg == "CMD:GET_PROFILES_FULL":
        profs = get_profiles_full()
        if profs:
            parts = [f"{s}|{p}" for s, p in profs]
            payload = ",".join(parts)
        else:
            payload = "NONE"
        send(ser, f"PROFILES_FULL:{payload}")

    elif msg.startswith("CMD:ADDWIFI_FULL:"):
        # Format: CMD:ADDWIFI_FULL:ssid|pass|dhcp|ip|sn|gw|dns
        rest  = msg[len("CMD:ADDWIFI_FULL:"):]
        parts = rest.split("|")
        if len(parts) >= 7:
            ssid   = parts[0]
            passwd = parts[1]
            dhcp   = parts[2] in ("1", "true", "yes")
            ip     = parts[3]
            sn     = parts[4]
            gw     = parts[5]
            dns    = parts[6]
            ok = add_wifi_full(ssid, passwd, dhcp, ip, sn, gw, dns)
            if ok:
                time.sleep(POST_CONNECT_WAIT)
                send_status(ser)
                send(ser, f"OK:ADDWIFI_FULL:{ssid}")
            else:
                send(ser, f"ERR:ADDWIFI_FULL:{ssid}")
        else:
            log.warning("CMD:ADDWIFI_FULL: bad format: %r", rest)
            send(ser, "ERR:ADDWIFI_FULL:bad_format")

    elif msg == "CMD:SCAN_WIFI":
        # Run in a thread so the serial loop stays responsive
        t = threading.Thread(target=scan_wifi, args=(ser,), daemon=True)
        t.start()

    elif msg == "CMD:START_AP":
        log.info("AP mode requested — handled by ESP32 locally")

    elif msg == "CMD:STOP_AP":
        log.info("AP mode stopped")

    elif msg == "CMD:TERM_OPEN":
        log.info("Terminal session requested.")
        open_terminal(ser)

    elif msg == "CMD:TERM_CLOSE":
        log.info("Terminal session closed by browser.")
        close_terminal()

    elif msg == "CMD:MSC_EJECT":
        err = eject_esp_msc()
        send(ser, "OK:MSC_EJECT" if err is None else f"ERR:MSC_EJECT:{err}")

    elif msg.startswith("TERM:IN:"):
        term_input(msg[8:])

    elif msg.startswith("CTL:RESIZE:"):
        # CTL:RESIZE:<cols>:<rows>
        try:
            _, cols_s, rows_s = msg[4:].split(":")
            term_resize(int(cols_s), int(rows_s))
        except ValueError:
            pass

    else:
        log.debug("Unknown message: %r", msg)


# ---------------------------------------------------------------------------
# Active serial connection
# ---------------------------------------------------------------------------
def handle_connection(ser: serial.Serial) -> None:
    log.info("Serial port open. Waiting for HELLO from ESP32...")
    line_buf    = ""
    last_ka     = time.time()
    initialized = False

    while True:
        try:
            waiting = ser.in_waiting
        except (serial.SerialException, OSError):
            log.warning("Serial connection lost.")
            break

        if waiting:
            try:
                raw = ser.read(waiting).decode("utf-8", errors="ignore")
            except (serial.SerialException, OSError):
                break
            line_buf += raw
            while "\n" in line_buf:
                line, line_buf = line_buf.split("\n", 1)
                line = line.strip()
                if line:
                    try:
                        dispatch(ser, line)
                    except serial.SerialException:
                        close_terminal()
                        return
                    if line == "HELLO":
                        initialized = True
                        last_ka = time.time()

        if initialized and (time.time() - last_ka) > KEEPALIVE_SECS:
            try:
                send_status(ser)
                last_ka = time.time()
            except serial.SerialException:
                break

        time.sleep(0.02)

    close_terminal()


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
def main() -> None:
    log.info("HMS Rig Console daemon starting.")
    log.info("Scanning for ESP32 on /dev/ttyACM* and /dev/ttyUSB* ...")

    while True:
        port = find_esp32_port()
        if not port:
            time.sleep(POLL_INTERVAL)
            continue

        log.info("ESP32 found on %s — opening port.", port)
        try:
            global _active_port
            _active_port = port
            with serial.Serial(port, BAUD, timeout=1) as ser:
                time.sleep(1.5)
                ser.reset_input_buffer()
                handle_connection(ser)
        except serial.SerialException as exc:
            log.error("Could not open %s: %s", port, exc)
        finally:
            _active_port = ""

        log.info("Device disconnected. Resuming scan...")
        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()
