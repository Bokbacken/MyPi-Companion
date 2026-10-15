#!/usr/bin/env bash
# Render the screen mockups (540x992, 3x device scale) to docs/images/.
# Usage: tools/screen_mockups/render.sh   (needs chromium)
set -e
cd "$(dirname "$0")"
for f in screen_*.html; do
  chromium-browser --headless --no-sandbox --disable-gpu --hide-scrollbars \
    --window-size=540,992 --screenshot="../../docs/images/${f%.html}.png" "file://$PWD/$f"
done
