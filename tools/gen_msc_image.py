#!/usr/bin/env python3
"""
gen_msc_image.py — build HMS_RigConsole_v5/msc_image.h from pi/ source files.

Usage:
    python3 tools/gen_msc_image.py

Run from the repo root.  The script reads:
    pi/install.sh
    pi/rig_daemon.py                        (stored on disk as DAEMON.PY)
    pi/pyserial-3.5-py2.py3-none-any.whl   (stored on disk as PYSERIAL.WHL)

And writes:
    HMS_RigConsole_v5/msc_image.h

The output is a FAT12 disk image (512 sectors × 512 bytes = 256 KB) encoded
as a C uint8_t array, identical in format to the original hand-built header.
"""

import math
import os
import struct
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
REPO_ROOT   = Path(__file__).resolve().parent.parent
PI_DIR      = REPO_ROOT / "pi"
OUTPUT_H    = REPO_ROOT / "HMS_RigConsole_v5" / "msc_image.h"

TOTAL_SECTORS   = 512
BYTES_PER_SECTOR = 512
TOTAL_BYTES     = TOTAL_SECTORS * BYTES_PER_SECTOR  # 262 144

# BPB (BIOS Parameter Block) — matches the original image exactly
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS    = 1
NUM_FATS            = 2
ROOT_ENTRIES        = 64
FAT_SECTORS         = 1          # 512 bytes → 340 addressable FAT12 entries
MEDIA_BYTE          = 0xF8
SECTORS_PER_TRACK   = 9
NUM_HEADS           = 2

# Derived offsets
FAT1_START   = RESERVED_SECTORS * BYTES_PER_SECTOR
FAT2_START   = FAT1_START + FAT_SECTORS * BYTES_PER_SECTOR
ROOT_START   = FAT2_START + FAT_SECTORS * BYTES_PER_SECTOR  # = 0x600
DATA_START   = ROOT_START + ROOT_ENTRIES * 32               # = 0xE00
FIRST_CLUSTER = 2   # cluster numbering starts at 2

# Files to embed on the disk (8.3 FAT name, source path, read-mode)
FILES = [
    (b"INSTALL SH ", PI_DIR / "install.sh",                            "rb"),
    (b"DAEMON  PY ", PI_DIR / "rig_daemon.py",                         "rb"),
    (b"PYSERIALWHL", PI_DIR / "pyserial-3.5-py2.py3-none-any.whl",    "rb"),
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def fat12_set(fat: bytearray, cluster: int, value: int) -> None:
    """Write a 12-bit value into a FAT12 table."""
    offset = (cluster * 3) // 2
    if cluster % 2 == 0:
        fat[offset]     = value & 0xFF
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F)
    else:
        fat[offset]     = (fat[offset] & 0x0F) | ((value & 0x0F) << 4)
        fat[offset + 1] = (value >> 4) & 0xFF


def make_boot_sector() -> bytes:
    """Return a 512-byte FAT12 boot sector."""
    bs = bytearray(512)
    # Jump + OEM
    bs[0:3] = b"\xEB\x3C\x90"
    bs[3:11] = b"MSDOS5.0"
    # BPB
    struct.pack_into("<H", bs, 11, BYTES_PER_SECTOR)
    bs[13] = SECTORS_PER_CLUSTER
    struct.pack_into("<H", bs, 14, RESERVED_SECTORS)
    bs[16] = NUM_FATS
    struct.pack_into("<H", bs, 17, ROOT_ENTRIES)
    struct.pack_into("<H", bs, 19, TOTAL_SECTORS)
    bs[21] = MEDIA_BYTE
    struct.pack_into("<H", bs, 22, FAT_SECTORS)
    struct.pack_into("<H", bs, 24, SECTORS_PER_TRACK)
    struct.pack_into("<H", bs, 26, NUM_HEADS)
    struct.pack_into("<I", bs, 28, 0)   # hidden sectors
    struct.pack_into("<I", bs, 32, 0)   # large total sectors
    # Extended BPB
    bs[36] = 0x00                       # drive number
    bs[37] = 0x00
    bs[38] = 0x29                       # extended boot signature
    struct.pack_into("<I", bs, 39, 0x12345678)  # volume serial
    bs[43:54] = b"RIGSETUP   "         # volume label (11 bytes)
    bs[54:62] = b"FAT12   "
    bs[510] = 0x55
    bs[511] = 0xAA
    return bytes(bs)


def make_dir_entry(name83: bytes, start_cluster: int, size: int) -> bytes:
    """Return a 32-byte FAT directory entry."""
    assert len(name83) == 11
    de = bytearray(32)
    de[0:11] = name83
    de[11]   = 0x20  # attribute: archive
    struct.pack_into("<H", de, 26, start_cluster)
    struct.pack_into("<I", de, 28, size)
    return bytes(de)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def build_image() -> bytes:
    image = bytearray(TOTAL_BYTES)

    # Boot sector
    image[0:512] = make_boot_sector()

    # Initialise both FAT copies (media byte)
    fat = bytearray(FAT_SECTORS * BYTES_PER_SECTOR)
    fat[0] = MEDIA_BYTE
    fat[1] = 0xFF
    fat[2] = 0xFF

    # Cluster allocator state
    next_cluster   = FIRST_CLUSTER
    dir_entries    = []

    for name83, src_path, mode in FILES:
        with open(src_path, mode) as fh:
            data = fh.read()
        size      = len(data)
        n_clusters = max(1, math.ceil(size / (SECTORS_PER_CLUSTER * BYTES_PER_SECTOR)))

        # Allocate clusters
        start_cluster = next_cluster
        chain = list(range(next_cluster, next_cluster + n_clusters))
        next_cluster += n_clusters

        # Write FAT chain
        for i, c in enumerate(chain):
            fat12_set(fat, c, chain[i + 1] if i + 1 < len(chain) else 0xFFF)

        # Write data into image
        for i, c in enumerate(chain):
            off   = DATA_START + (c - FIRST_CLUSTER) * SECTORS_PER_CLUSTER * BYTES_PER_SECTOR
            chunk = data[i * SECTORS_PER_CLUSTER * BYTES_PER_SECTOR :
                         (i + 1) * SECTORS_PER_CLUSTER * BYTES_PER_SECTOR]
            chunk = chunk.ljust(SECTORS_PER_CLUSTER * BYTES_PER_SECTOR, b"\x00")
            image[off : off + len(chunk)] = chunk

        dir_entries.append(make_dir_entry(name83, start_cluster, size))
        print(f"  {src_path.name:45s} {size:6d} B  clusters {start_cluster}–{chain[-1]}")

    # Write FAT (both copies)
    image[FAT1_START : FAT1_START + len(fat)] = fat
    image[FAT2_START : FAT2_START + len(fat)] = fat

    # Write root directory
    for i, de in enumerate(dir_entries):
        off = ROOT_START + i * 32
        image[off : off + 32] = de

    return bytes(image)


def write_header(image: bytes, out_path: Path) -> None:
    sectors = len(image) // BYTES_PER_SECTOR
    lines = [
        "#pragma once",
        f"static const uint32_t MSC_IMAGE_SECTORS = {sectors}u;",
        f"static const uint32_t MSC_BLOCK_SIZE    = {BYTES_PER_SECTOR}u;",
        f"static const uint8_t  MSC_IMAGE[] = {{",
    ]
    for i in range(0, len(image), 16):
        chunk = image[i : i + 16]
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="ascii")
    print(f"\nWritten: {out_path}  ({len(image)} bytes, {sectors} sectors)")


if __name__ == "__main__":
    print("Building MSC disk image...")
    for _, src, _ in FILES:
        if not src.exists():
            print(f"ERROR: missing source file: {src}", file=sys.stderr)
            sys.exit(1)

    image = build_image()
    write_header(image, OUTPUT_H)
    print("Done.")
