// ============================================================
//  sd_mgr.ino — SD card manager (SD_MMC, 1-bit mode)
//
//  Waveshare ESP32-S3-Touch-LCD-1.47 TF slot:
//    CLK = GPIO16   CMD = GPIO15   D0 = GPIO17   (1-bit bus)
//
//  SD Share exposes the raw card to the Pi through the USB MSC
//  interface (msc.ino, MSC_BACKEND_SD).  While sharing is active
//  the ESP must not touch the card's filesystem, so all file
//  helpers below refuse to run.  After sharing stops the card is
//  remounted to drop any stale FAT cache.
//
//  NVS: namespace "sdmgr", key "sd_on_boot" (bool) — start SD
//  Share automatically at boot.
// ============================================================

#include <SD_MMC.h>
#include <Preferences.h>

#define SD_PIN_CLK  16
#define SD_PIN_CMD  15
#define SD_PIN_D0   17

static bool _sdMounted = false;
static bool _sdOnBoot  = false;

// ── Mount / unmount ───────────────────────────────────────────

bool sdMount() {
  if (_sdMounted) return true;
  // mode1bit = true, no auto-format.  Single attempt — never retry
  // here: a blocking retry loop would stall the HTTP server if the
  // SDMMC peripheral is in a bad state (e.g. right after OTA reset).
  bool ok = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT);
  if (!ok) {
    Serial.println("SD: mount failed (no card?)");
    return false;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    Serial.println("SD: no card");
    return false;
  }
  _sdMounted = true;
  Serial.printf("SD: mounted, %llu MB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
  return true;
}

void sdUnmount() {
  if (sdIsShareActive()) sdShareStop();
  if (_sdMounted) SD_MMC.end();
  _sdMounted = false;
}

bool sdIsMounted()     { return _sdMounted; }
bool sdIsOnBoot()      { return _sdOnBoot; }
bool sdIsShareActive() { return mscBackendIsSD(); }

// ── Raw sector access (used by msc.ino) ───────────────────────

uint32_t sdSectorCount() {
  if (!_sdMounted) return 0;
  if (SD_MMC.sectorSize() != 512) return 0;
  // NOTE: SD_MMC.numSectors() returns the *filesystem* size (totalBytes),
  // which excludes FAT tables/reserved area. MSC must expose the whole
  // physical card, so derive the count from the card capacity (CSD).
  return (uint32_t)(SD_MMC.cardSize() / 512ULL);
}

bool sdReadSector(uint8_t *buf, uint32_t sector) {
  return _sdMounted && SD_MMC.readRAW(buf, sector);
}

bool sdWriteSector(uint8_t *buf, uint32_t sector) {
  return _sdMounted && SD_MMC.writeRAW(buf, sector);
}

// ── SD Share ──────────────────────────────────────────────────

bool sdShareStart() {
  if (sdIsShareActive()) return true;
  if (!sdMount()) return false;
  return mscSetBackendSD();
}

void sdShareStop() {
  if (!sdIsShareActive()) return;
  mscSetBackendNone();
  // The host may have changed the filesystem — remount for a fresh view.
  if (_sdMounted) { SD_MMC.end(); _sdMounted = false; }
  delay(200);   // let the card settle after the last host write
  sdMount();
}

void sdSetOnBoot(bool enable) {
  Preferences p;
  if (p.begin("sdmgr", false)) {
    p.putBool("sd_on_boot", enable);
    p.end();
  }
  _sdOnBoot = enable;
}

// Called from mscBegin() (before USB.begin()).
void sdInit() {
  Preferences p;
  if (p.begin("sdmgr", true)) {
    _sdOnBoot = p.getBool("sd_on_boot", false);
    p.end();
  }
  SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0);
  sdMount();
  if (_sdOnBoot && _sdMounted) {
    Serial.println("SD: Share enabled on boot");
    sdShareStart();
  }
}

// ── File helpers (refuse while shared with the host) ──────────

static bool _sdFsUsable() { return _sdMounted && !sdIsShareActive(); }

static String _sdJsonEsc(const String &s) {
  String out;
  out.reserve(s.length() + 4);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if      (c == '"')  out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}

static void _sdListRec(fs::FS &fs, const String &dirPath, String &json,
                       bool &first, int depth) {
  File dir = fs.open(dirPath);
  if (!dir || !dir.isDirectory()) return;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String name = f.name();
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    String path = dirPath.endsWith("/") ? dirPath + name : dirPath + "/" + name;
    bool isDir = f.isDirectory();
    if (!first) json += ",";
    first = false;
    json += "{\"name\":\"";  json += _sdJsonEsc(name);
    json += "\",\"path\":\""; json += _sdJsonEsc(path);
    json += "\",\"size\":";  json += isDir ? 0 : (uint32_t)f.size();
    json += ",\"dir\":";     json += isDir ? "true" : "false";
    json += "}";
    f.close();
    if (isDir && depth < 6) _sdListRec(fs, path, json, first, depth + 1);
  }
  dir.close();
}

// Append JSON objects {name,path,size,dir} for every entry below dirPath
// (recursive) to json.  Works for any fs::FS (SD_MMC or FFat).
// first tracks comma placement across calls.
bool sdListDir(fs::FS &fs, const char *dirPath, String &json, bool &first) {
  if (&fs == (fs::FS *)&SD_MMC && !_sdFsUsable()) return false;
  _sdListRec(fs, String(dirPath), json, first, 0);
  return true;
}

bool sdReadFile(const char *path, String &out) {
  if (!_sdFsUsable()) return false;
  File f = SD_MMC.open(path, FILE_READ);
  if (!f || f.isDirectory()) return false;
  out = f.readString();
  f.close();
  return true;
}

bool sdWriteFile(const char *path, const uint8_t *data, size_t len, bool append) {
  if (!_sdFsUsable()) return false;
  File f = SD_MMC.open(path, append ? FILE_APPEND : FILE_WRITE);
  if (!f) return false;
  size_t w = f.write(data, len);
  f.close();
  return w == len;
}

// Delete a file, or a directory including its contents.
static bool _fsRemoveRec(fs::FS &fs, const String &path, int depth) {
  File f = fs.open(path);
  if (!f) return false;
  if (!f.isDirectory()) { f.close(); return fs.remove(path); }
  if (depth > 6) { f.close(); return false; }
  for (File c = f.openNextFile(); c; c = f.openNextFile()) {
    String name = c.name();
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    c.close();
    _fsRemoveRec(fs, path + "/" + name, depth + 1);
  }
  f.close();
  return fs.rmdir(path);
}

bool sdDeleteFile(const char *path) {
  if (!_sdFsUsable()) return false;
  return _fsRemoveRec(SD_MMC, String(path), 0);
}
