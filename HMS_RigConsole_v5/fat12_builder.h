// ============================================================
//  fat12_builder.h — build a FAT12 disk image in RAM from a
//  directory on FFat (C++ port of tools/gen_msc_image.py).
//
//  Layout (512-byte sectors, 1 sector per cluster):
//    sector 0      boot sector  (volume label "RIGSETUP   ")
//    sectors 1-2   FAT #1
//    sectors 3-4   FAT #2
//    sectors 5-8   root directory (64 entries)
//    sector 9+     data area (cluster 2 = sector 9)
// ============================================================
#pragma once
#include <stdint.h>
#include <stddef.h>

#define FAT12_SECTOR_SIZE   512
#define FAT12_FAT_SECTORS   2
#define FAT12_ROOT_ENTRIES  64
#define FAT12_ROOT_SECTORS  ((FAT12_ROOT_ENTRIES * 32) / FAT12_SECTOR_SIZE)
#define FAT12_DATA_START    (1 + 2 * FAT12_FAT_SECTORS + FAT12_ROOT_SECTORS)  // = 9

// Build a FAT12 image from all regular files directly inside ffatDir
// (e.g. "/rigsetup").  The image fills bufLen (rounded down to whole
// sectors).  Returns false if the directory is missing/empty, files do
// not fit, or a read error occurs.  *outSize receives the image size.
bool fat12BuildFromDir(const char *ffatDir, uint8_t *buf, size_t bufLen,
                       uint32_t *outSize);
