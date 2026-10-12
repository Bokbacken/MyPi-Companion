#!/usr/bin/env bash
# ============================================================
#  MyPi Companion — Pi-side uninstaller
#
#  Removes the rig-console daemon, its systemd service, and
#  the installed files. Leaves the dialout group membership and
#  pyserial in place (they are harmless and may be shared).
#
#  Usage: sudo bash uninstall.sh
#
#  Optional: pass --purge to also remove the install directory
#            even if it contains unexpected files.
# ============================================================

set -euo pipefail

INSTALL_DIR="/opt/rig-console"
SERVICE_DAEMON="rig-console"
SERVICE_TERM="rig-terminal"
PURGE=false

for arg in "$@"; do
  [[ "$arg" == "--purge" ]] && PURGE=true
done

echo "=== MyPi Companion — Uninstallation ==="
echo "    Install dir : $INSTALL_DIR"
echo "    Purge       : $PURGE"
echo ""

# ── Require root ──────────────────────────────────────────────
if [[ $EUID -ne 0 ]]; then
  echo "ERROR: Run with sudo:  sudo bash uninstall.sh"
  exit 1
fi

# ── 1. Stop and disable services ──────────────────────────────
echo "[1/3] Stopping and disabling services..."
for svc in "$SERVICE_DAEMON" "$SERVICE_TERM"; do
  if systemctl list-unit-files "${svc}.service" &>/dev/null; then
    systemctl stop    "${svc}.service" 2>/dev/null || true
    systemctl disable "${svc}.service" 2>/dev/null || true
    echo "      Stopped: $svc"
  else
    echo "      Not found (skipped): $svc"
  fi
done

# ── 2. Remove service unit files ──────────────────────────────
echo "[2/3] Removing systemd unit files..."
for svc in "$SERVICE_DAEMON" "$SERVICE_TERM"; do
  unit="/etc/systemd/system/${svc}.service"
  if [[ -f "$unit" ]]; then
    rm -f "$unit"
    echo "      Removed: $unit"
  fi
done
systemctl daemon-reload

# ── 3. Remove install directory ───────────────────────────────
echo "[3/3] Removing $INSTALL_DIR ..."
if [[ -d "$INSTALL_DIR" ]]; then
  if $PURGE; then
    rm -rf "$INSTALL_DIR"
    echo "      Purged: $INSTALL_DIR"
  else
    # Only remove the known daemon file to avoid deleting user data
    rm -f "$INSTALL_DIR/rig_daemon.py"
    # Remove dir if now empty
    rmdir "$INSTALL_DIR" 2>/dev/null && echo "      Removed: $INSTALL_DIR" \
      || echo "      Directory not empty — left in place (use --purge to force)"
  fi
else
  echo "      Not found (skipped): $INSTALL_DIR"
fi

echo ""
echo "==========================================="
echo "  Uninstallation complete."
echo "==========================================="
echo ""
echo "  NOTE: The 'dialout' group membership for your user was NOT removed."
echo "        Run 'sudo gpasswd -d \$USER dialout' to remove it manually."
echo ""
