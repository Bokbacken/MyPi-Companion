#!/usr/bin/env bash
# ============================================================
#  HMS Rig Console v4 — Pi-side installer
#
#  Usage (manual):   sudo bash install.sh
#  Usage (via HID):  sudo bash /media/<user>/RIGSETUP/install.sh /dev/ttyACMx
#                    sudo bash /mnt/rigsetup/install.sh /dev/ttyACMx   (CLI mode,
#                    read-only mount; this script never writes to its own disk)
#
#  $1 (optional) — feedback serial port.
#  When supplied the script echoes RIG:* status codes to that
#  port so the ESP32 can update its display in real time.
#
#  Status codes:
#    RIG:INSTALL_START
#    RIG:INSTALL_DEPS
#    RIG:INSTALL_DAEMON
#    RIG:INSTALL_SERVICE
#    RIG:INSTALL_DONE
#    RIG:INSTALL_ERR_NOROOT
#    RIG:INSTALL_ERR_DEPS
#    RIG:INSTALL_ERR_COPY
#    RIG:INSTALL_ERR_SERVICE
# ============================================================

set -euo pipefail

FEEDBACK="${1:-}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

INSTALL_DIR="/opt/rig-console"
SERVICE_DAEMON="rig-console"
SERVICE_TERM="rig-terminal"
RUN_USER="${SUDO_USER:-pi}"

# ── Send status to ESP32 and stdout ───────────────────────────
fb() {
  echo "$1"
  if [ -n "$FEEDBACK" ] && [ -c "$FEEDBACK" ]; then
    echo "$1" > "$FEEDBACK" 2>/dev/null || true
  fi
}

echo "=== HMS Rig Console — Installation ==="
echo "    Install user : $RUN_USER"
echo "    Install dir  : $INSTALL_DIR"
echo ""

fb "RIG:INSTALL_START"

# ── Require root ──────────────────────────────────────────────
if [[ $EUID -ne 0 ]]; then
  fb "RIG:INSTALL_ERR_NOROOT"
  echo "ERROR: Run with sudo:  sudo bash install.sh"
  exit 1
fi

# ── 0. Stop existing service before touching any files ────────
#  Ensures the running daemon releases /dev/ttyACM0 and its
#  file handle on rig_daemon.py before we overwrite it.
echo "[0/5] Stopping existing daemon (if running)..."
systemctl stop    "$SERVICE_DAEMON" 2>/dev/null || true
systemctl disable "$SERVICE_DAEMON" 2>/dev/null || true
echo "      OK"

# ── 1. Python dependencies (offline — wheel bundled on disk) ──
fb "RIG:INSTALL_DEPS"
echo "[1/5] Installing pyserial (offline)..."
# The pyserial wheel is bundled on the RIGSETUP disk — no internet needed.
# -iname: FAT short names may be shown in upper case (PYSERIAL.WHL).
WHL=$(find "$SCRIPT_DIR" -iname 'pyserial.whl' | sort | tail -1)
if python3 -c 'import serial' 2>/dev/null; then
  echo "      pyserial already installed"
elif [ -n "$WHL" ]; then
  # pip only accepts wheels with a valid PEP 427 file name, so copy the
  # 8.3-named file to its real name first.
  TMP_WHL="/tmp/pyserial-3.5-py2.py3-none-any.whl"
  cp -f "$WHL" "$TMP_WHL"
  # --break-system-packages is needed on Bookworm (PEP 668) and unknown to old pip.
  python3 -m pip install --quiet --no-index "$TMP_WHL" 2>/dev/null \
    || python3 -m pip install --quiet --no-index --break-system-packages "$TMP_WHL" \
    || true
  rm -f "$TMP_WHL"
else
  # Fallback: try apt (requires internet)
  apt-get install -y -q python3-serial || true
fi
# Raspberry Pi OS Lite ships without pip, so the pip step above may have
# done nothing. pyserial is pure Python: unpack the wheel into a private
# lib dir instead; the service adds it to PYTHONPATH.
mkdir -p "$INSTALL_DIR/lib"
if ! python3 -c 'import serial' 2>/dev/null && [ -n "$WHL" ]; then
  echo "      pip unavailable - unpacking wheel to $INSTALL_DIR/lib"
  python3 -m zipfile -e "$WHL" "$INSTALL_DIR/lib"
fi
if ! PYTHONPATH="$INSTALL_DIR/lib" python3 -c 'import serial' 2>/dev/null; then
  fb "RIG:INSTALL_ERR_DEPS"
  echo "ERROR: pyserial could not be installed"
  exit 1
fi
echo "      OK"

# ── 2. Copy daemon ────────────────────────────────────────────
fb "RIG:INSTALL_DAEMON"
echo "[2/5] Copying daemon to $INSTALL_DIR ..."
mkdir -p "$INSTALL_DIR"
# On the RIGSETUP disk the file is stored as 'daemon.py' (8.3 FAT name).
if ! cp -f "$SCRIPT_DIR/daemon.py" "$INSTALL_DIR/rig_daemon.py"; then
  fb "RIG:INSTALL_ERR_COPY"
  exit 1
fi
chmod 755 "$INSTALL_DIR/rig_daemon.py"
echo "      OK"

# ── 3. dialout group ──────────────────────────────────────────
echo "[3/5] Adding $RUN_USER to dialout..."
usermod -aG dialout "$RUN_USER"
echo "      OK"

# ── 4. Systemd service (inlined) ──────────────────────────────
fb "RIG:INSTALL_SERVICE"
echo "[4/5] Installing systemd service..."

cat > /etc/systemd/system/${SERVICE_DAEMON}.service << SVCEOF
[Unit]
Description=HMS Rig Console Daemon
# NOTE: BindsTo is intentionally omitted.
#   BindsTo=dev-ttyACM0.device would cause systemd to kill the service
#   the moment the USB device re-enumerates or disconnects — preventing
#   the daemon's own reconnect loop from recovering the connection.
#   The daemon polls for the port itself and handles reconnection.

[Service]
User=$RUN_USER
# Private lib dir: pyserial unpacked from the bundled wheel (no pip on Lite)
Environment=PYTHONPATH=$INSTALL_DIR/lib
ExecStart=/usr/bin/python3 $INSTALL_DIR/rig_daemon.py
# Restart=always so systemd restarts on both failures AND clean exits
# (a clean exit after USB disconnect would not restart with on-failure).
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
SVCEOF

if ! systemctl daemon-reload; then
  fb "RIG:INSTALL_ERR_SERVICE"
  exit 1
fi

systemctl enable  "$SERVICE_DAEMON"
systemctl restart "$SERVICE_DAEMON" || true
echo "      OK"

fb "RIG:INSTALL_DONE"

echo ""
echo "==========================================="
echo "  Installation complete!"
echo "==========================================="
echo ""
echo "  Rig daemon  : sudo systemctl status $SERVICE_DAEMON"
echo "  Logs        : journalctl -u $SERVICE_DAEMON -f"
echo ""
echo "  NOTE: Log out and back in (or reboot) for dialout group to take effect."
echo ""
