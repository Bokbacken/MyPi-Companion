"""Generate MyPi Companion logo with an exact UART 8N1 frame (LSB first) and logo_bitmaps.h."""
from PIL import Image
import numpy as np
import itertools
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "tools", "assets")

BYTE = 0x43          # 'C' = Companion
BIT = 45             # px per bit period (source artwork scale)
GREEN = [3, 167, 83, 255]
X0, X1 = 230, 793
HY, LY = (670, 673), (704, 707)
VW = 4

src = Image.open(os.path.join(ASSETS, "mypi_companion_logo_src.png")).convert("RGBA")
arr = np.array(src, dtype=np.uint8)
arr[662:716, 222:802] = [255, 255, 255, 255]   # erase old wave incl. anti-aliasing

data = [(BYTE >> i) & 1 for i in range(8)]     # LSB first
frame = [0] + data + [1]                        # START, D0..D7, STOP
W = X1 - X0 + 1
pre = (W - 10 * BIT) // 2
post = W - 10 * BIT - pre
levels = [1] * pre + sum([[b] * BIT for b in frame], []) + [1] * post
for i, l in enumerate(levels):
    x = X0 + i
    y = HY if l else LY
    arr[y[0]:y[1] + 1, x] = GREEN
    if i > 0 and levels[i - 1] != l:
        arr[HY[0]:LY[1] + 1, x:x + VW] = GREEN

out = Image.fromarray(arr)
out.save(os.path.join(ASSETS, "mypi_companion_logo.png"))
print("D0..D7:", data)
print([(k, len(list(g))) for k, g in itertools.groupby(levels)])


def onwhite(c):
    b = Image.new("RGB", c.size, (255, 255, 255))
    b.paste(c, mask=c.split()[3])
    return b


def c_arr(img, name):
    a = np.array(img, dtype=np.uint16)
    h, w = a.shape[:2]
    d = (((a[:, :, 0] >> 3) << 11) | ((a[:, :, 1] >> 2) << 5) | (a[:, :, 2] >> 3)).flatten()
    rows = ["  " + ", ".join(f"0x{v:04X}" for v in d[i:i + 12]) for i in range(0, len(d), 12)]
    return (f"// {name} — {w}x{h} px, RGB565\n#define {name.upper()}_W {w}\n"
            f"#define {name.upper()}_H {h}\nstatic const uint16_t {name}[{w * h}] PROGMEM = {{\n"
            + ",\n".join(rows) + "\n};")


a = np.array(out)
m = ~((a[:, :, 0] > 240) & (a[:, :, 1] > 240) & (a[:, :, 2] > 240))
r = np.where(m.any(1))[0]
c = np.where(m.any(0))[0]
full = onwhite(out.crop((c[0] - 20, r[0] - 20, c[-1] + 20, r[-1] + 20)))
full.thumbnail((172, 320), Image.LANCZOS)
splash = Image.new("RGB", (172, 320), (0, 0, 0))
splash.paste(full, ((172 - full.width) // 2, (320 - full.height) // 2))

sym = onwhite(out.crop((199, 206, 824, 696)))
sym.thumbnail((100, 18), Image.LANCZOS)
icon = Image.new("RGB", (sym.width, 18), (0, 0, 0))
icon.paste(sym, (0, (18 - sym.height) // 2))

hdr = ("// ============================================================\n"
       "//  logo_bitmaps.h — MyPi Companion splash + header icon\n"
       "//  AUTO-GENERATED — do not edit manually.\n"
       "//\n"
       "//  Green waveform = one UART frame, 8N1 (8 data bits, no parity,\n"
       "//  1 stop bit), transmitted LSB first:\n"
       "//    idle(H) START(L) D0 D1 D2 D3 D4 D5 D6 D7 STOP(H) idle(H)\n"
       "//  Byte 0x43 = 'C' (Companion) -> D0..D7 = 1 1 0 0 0 0 1 0\n"
       "//  Rendered at 45 px per bit period in the 1024 px source artwork.\n"
       "//  Other MyPi tools use their own byte in the same frame format.\n"
       "//\n"
       "//  Format: RGB565, PROGMEM\n"
       "// ============================================================\n"
       "#pragma once\n#include <pgmspace.h>\n\n")
open(os.path.join(ROOT, "MyPi_Companion_V6", "logo_bitmaps.h"), "w").write(hdr + c_arr(splash, "logo_splash") + "\n\n" + c_arr(icon, "logo_icon") + "\n")
print("icon", icon.size)
