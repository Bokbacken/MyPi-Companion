// ============================================================
//  fat12_builder.ino — build the RIGSETUP FAT12 image at runtime
//
//  C++ port of the former tools/gen_msc_image.py.  Instead of baking the Pi
//  install files into flash, the image is assembled in PSRAM from
//  the regular files found in /rigsetup/ on the FFat partition.
//
//  FFat layout convention:
//    /config/    settings (macros.json, menus.json, ...)
//    /rigsetup/  files exposed to the Pi on the RIGSETUP USB disk
//                (install.sh, daemon.py, pyserial.whl)
//    /data/      reserved for future use
//
//  File names are converted to 8.3 (no long-file-name entries).
//  All-lowercase names keep their case on Linux/Windows through
//  the NT case flags (byte 12 of the directory entry).
// ============================================================

#include <FFat.h>
#include "fat12_builder.h"

static const uint8_t  FAT12_MEDIA_BYTE = 0xF8;
static const uint32_t FAT12_FIRST_CLUSTER = 2;
// Highest cluster number addressable by FAT12_FAT_SECTORS of FAT:
// (2 * 512 * 2 / 3) = 682 entries → clusters 0..681
static const uint32_t FAT12_MAX_CLUSTER =
    (FAT12_FAT_SECTORS * FAT12_SECTOR_SIZE * 2) / 3 - 1;

static inline void _fat12Put16(uint8_t *p, uint16_t v) {
  p[0] = v & 0xFF; p[1] = v >> 8;
}
static inline void _fat12Put32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = v >> 24;
}

// Write a 12-bit value into a FAT12 table.
static void _fat12Set(uint8_t *fat, uint32_t cluster, uint16_t value) {
  uint32_t off = (cluster * 3) / 2;
  if ((cluster & 1) == 0) {
    fat[off]     = value & 0xFF;
    fat[off + 1] = (fat[off + 1] & 0xF0) | ((value >> 8) & 0x0F);
  } else {
    fat[off]     = (fat[off] & 0x0F) | ((value & 0x0F) << 4);
    fat[off + 1] = (value >> 4) & 0xFF;
  }
}

static void _fat12BootSector(uint8_t *bs, uint16_t totalSectors) {
  memset(bs, 0, FAT12_SECTOR_SIZE);
  bs[0] = 0xEB; bs[1] = 0x3C; bs[2] = 0x90;
  memcpy(bs + 3, "MSDOS5.0", 8);
  _fat12Put16(bs + 11, FAT12_SECTOR_SIZE);
  bs[13] = 1;                                  // sectors per cluster
  _fat12Put16(bs + 14, 1);                     // reserved sectors
  bs[16] = 2;                                  // number of FATs
  _fat12Put16(bs + 17, FAT12_ROOT_ENTRIES);
  _fat12Put16(bs + 19, totalSectors);
  bs[21] = FAT12_MEDIA_BYTE;
  _fat12Put16(bs + 22, FAT12_FAT_SECTORS);
  _fat12Put16(bs + 24, 1);                     // sectors per track
  _fat12Put16(bs + 26, 1);                     // heads
  _fat12Put32(bs + 28, 0);                     // hidden sectors
  _fat12Put32(bs + 32, 0);                     // large total sectors
  bs[36] = 0x80;                               // drive number
  bs[38] = 0x29;                               // extended boot signature
  bs[39] = 0xDE; bs[40] = 0xAD; bs[41] = 0xBE; bs[42] = 0xEF;
  memcpy(bs + 43, "RIGSETUP   ", 11);
  memcpy(bs + 54, "FAT12   ", 8);
  bs[510] = 0x55; bs[511] = 0xAA;
}

static char _fat12Char(char c) {
  if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
  if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
  if (strchr("!#$%&'()-@^_`{}~", c)) return c;
  return '_';
}

// Convert a file name to a space-padded 11-byte 8.3 name.
// Returns the NT case flags (0x08 = base lowercase, 0x10 = ext lowercase).
static uint8_t _fat12Name83(const char *name, char out[11]) {
  memset(out, ' ', 11);
  const char *dot = strrchr(name, '.');
  if (dot == name) dot = nullptr;              // ".hidden" → no extension
  size_t baseLen = dot ? (size_t)(dot - name) : strlen(name);

  bool baseLower = false, baseUpper = false, extLower = false, extUpper = false;
  int n = 0;
  for (size_t i = 0; i < baseLen && n < 8; i++) {
    char c = name[i];
    if (c == ' ' || c == '.') continue;
    if (c >= 'a' && c <= 'z') baseLower = true;
    if (c >= 'A' && c <= 'Z') baseUpper = true;
    out[n++] = _fat12Char(c);
  }
  if (n == 0) out[0] = '_';
  if (dot) {
    n = 0;
    for (const char *p = dot + 1; *p && n < 3; p++) {
      if (*p == ' ') continue;
      if (*p >= 'a' && *p <= 'z') extLower = true;
      if (*p >= 'A' && *p <= 'Z') extUpper = true;
      out[8 + n++] = _fat12Char(*p);
    }
  }
  uint8_t flags = 0;
  if (baseLower && !baseUpper) flags |= 0x08;
  if (extLower && !extUpper)   flags |= 0x10;
  return flags;
}

bool fat12BuildFromDir(const char *ffatDir, uint8_t *buf, size_t bufLen,
                       uint32_t *outSize) {
  if (outSize) *outSize = 0;
  if (!buf || !ffatDir) return false;

  // Size the volume to the buffer, capped at what the FAT can address.
  uint32_t totalSectors = bufLen / FAT12_SECTOR_SIZE;
  const uint32_t maxSectors = FAT12_DATA_START + (FAT12_MAX_CLUSTER - 1);
  if (totalSectors > maxSectors) totalSectors = maxSectors;
  if (totalSectors <= FAT12_DATA_START) return false;
  const uint32_t lastCluster = FAT12_FIRST_CLUSTER + (totalSectors - FAT12_DATA_START) - 1;

  File dir = FFat.open(ffatDir);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("fat12: %s not found\n", ffatDir);
    return false;
  }

  memset(buf, 0, (size_t)totalSectors * FAT12_SECTOR_SIZE);
  _fat12BootSector(buf, (uint16_t)totalSectors);

  uint8_t *fat  = buf + 1 * FAT12_SECTOR_SIZE;
  uint8_t *root = buf + (1 + 2 * FAT12_FAT_SECTORS) * FAT12_SECTOR_SIZE;
  fat[0] = FAT12_MEDIA_BYTE; fat[1] = 0xFF; fat[2] = 0xFF;

  // Root entry 0: volume label (Linux reads the label from here)
  memcpy(root, "RIGSETUP   ", 11);
  root[11] = 0x08;

  uint32_t nextCluster = FAT12_FIRST_CLUSTER;
  int      entry       = 1;
  int      nFiles      = 0;
  bool     ok          = true;

  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) { f.close(); continue; }
    if (entry >= FAT12_ROOT_ENTRIES) {
      Serial.println("fat12: too many files");
      f.close(); ok = false; break;
    }
    const char *fname = f.name();
    const char *slash = strrchr(fname, '/');
    if (slash) fname = slash + 1;

    char    name83[11];
    uint8_t ntFlags = _fat12Name83(fname, name83);

    // Resolve 8.3 collisions with a ~N suffix on the base name.
    for (int tries = 1; tries < 10; tries++) {
      bool clash = false;
      for (int e = 1; e < entry; e++)
        if (memcmp(root + e * 32, name83, 11) == 0) { clash = true; break; }
      if (!clash) break;
      int len = 8;
      while (len > 0 && name83[len - 1] == ' ') len--;
      if (len > 6) len = 6;
      name83[len] = '~'; name83[len + 1] = '0' + tries;
      ntFlags &= ~0x08;
    }

    uint32_t size      = f.size();
    uint32_t nClusters = size ? (size + FAT12_SECTOR_SIZE - 1) / FAT12_SECTOR_SIZE : 0;
    uint32_t start     = nClusters ? nextCluster : 0;
    if (nClusters && start + nClusters - 1 > lastCluster) {
      Serial.printf("fat12: %s does not fit\n", fname);
      f.close(); ok = false; break;
    }

    // Read file data straight into the data area (contiguous chain).
    if (nClusters) {
      uint8_t *dst = buf + (FAT12_DATA_START + (start - FAT12_FIRST_CLUSTER)) * FAT12_SECTOR_SIZE;
      size_t got = f.read(dst, size);
      if (got != size) {
        Serial.printf("fat12: read error on %s\n", fname);
        f.close(); ok = false; break;
      }
      for (uint32_t i = 0; i < nClusters; i++) {
        uint32_t c = start + i;
        _fat12Set(fat, c, (i + 1 < nClusters) ? (uint16_t)(c + 1) : 0xFFF);
      }
      nextCluster += nClusters;
    }

    uint8_t *de = root + entry * 32;
    memcpy(de, name83, 11);
    de[11] = 0x20;                 // archive
    de[12] = ntFlags;              // NT lowercase flags
    _fat12Put16(de + 26, (uint16_t)start);
    _fat12Put32(de + 28, size);
    entry++; nFiles++;
    Serial.printf("fat12: %-24s %7u B  -> %.11s\n", fname, (unsigned)size, name83);
    f.close();
  }
  dir.close();

  if (!ok || nFiles == 0) return false;

  // Second FAT copy
  memcpy(fat + FAT12_FAT_SECTORS * FAT12_SECTOR_SIZE, fat,
         FAT12_FAT_SECTORS * FAT12_SECTOR_SIZE);

  if (outSize) *outSize = totalSectors * FAT12_SECTOR_SIZE;
  return true;
}
