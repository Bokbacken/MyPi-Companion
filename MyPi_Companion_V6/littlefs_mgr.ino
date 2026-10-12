// ============================================================
//  littlefs_mgr.ino — FFat initialisation + HTTP file handlers
//
//  Uses ESP32 FFat (FAT over flash) with the predefined partition scheme:
//    Arduino IDE → Tools → Partition Scheme → "16M Flash (3MB APP/9.9MB FATFS)"
//  No custom partitions.csv — one in the sketch folder overrides the IDE scheme.
//
//  Routes registered in web_server.ino::startWebServer():
//    GET  /api/files      — JSON list of files [{name, size}, …]
//    POST /api/files/del  — delete a file  (field: name)
//    POST /upload         — upload a file  (multipart field: file)
//
//  Files are served via onNotFound() → handleFileServe():
//    GET  /files/<name>   — stream a file from FFat
//
//  Convention: config JSON files live under /config/ on FFat
//  (/config/macros.json, /config/menus.json).  Legacy root-level
//  copies are migrated once at boot by littleFsBegin().
//  /rigsetup/ holds the files of the RIGSETUP USB disk; any file
//  missing there is restored at boot from rigsetup_files.h
//  (generated from pi/ by tools/gen_rigsetup_h.py).
//
//  `server` is declared in MyPi_Companion_V6.ino and visible here
//  because Arduino merges all .ino files into one compilation unit.
// ============================================================

#include "rigsetup_files.h"   // RIGSETUP_FILES[] — firmware defaults for /rigsetup/

static bool _fatOk = false;   // set true only if FFat.begin() succeeds

// JSON-escape helper (local — web_server.ino's jsonEscape() is static)
static String _fsJsonEsc(const String &s) {
  String out;
  out.reserve(s.length() + 4);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if      (c == '"')  out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else                out += c;
  }
  return out;
}

// Copy src → dst in 512-byte chunks, then remove src.
// Only runs if src exists and dst does not.  On a failed copy the
// partial dst is removed and src is kept.
static void _fsMigrate(const char *src, const char *dst) {
  if (!FFat.exists(src) || FFat.exists(dst)) return;

  File in  = FFat.open(src, "r");
  File out = FFat.open(dst, "w");
  bool ok  = in && out;
  uint8_t buf[512];
  while (ok && in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (n == 0 || out.write(buf, n) != n) ok = false;
  }
  if (in)  in.close();
  if (out) out.close();

  if (ok) {
    FFat.remove(src);
    Serial.printf("FFat: migrated %s -> %s\n", src, dst);
  } else {
    FFat.remove(dst);
    Serial.printf("ERR: FFat migration failed %s -> %s\n", src, dst);
  }
}

// ============================================================
//  littleFsBegin()  — called once from setup()
//  (name kept so MyPi_Companion_V6.ino needs no further change)
// ============================================================
// Populate /rigsetup/ from the compiled-in arrays: writes every file
// that is missing (first boot, after an FFat format, or after the user
// deleted it).  Existing files are left alone, so edits made via the
// Storage page are kept.
static void _ensureRigsetupFiles() {
  if (!FFat.exists("/rigsetup")) FFat.mkdir("/rigsetup");
  bool anyMissing = false;
  for (size_t i = 0; i < RIGSETUP_FILE_COUNT; i++) {
    char path[64];
    snprintf(path, sizeof(path), "/rigsetup/%s", RIGSETUP_FILES[i].name);
    if (!FFat.exists(path)) { anyMissing = true; break; }
  }
  if (!anyMissing) return;  // all files present
  Serial.println("FFat: populating /rigsetup/ from firmware defaults");
  for (size_t i = 0; i < RIGSETUP_FILE_COUNT; i++) {
    char path[64];
    snprintf(path, sizeof(path), "/rigsetup/%s", RIGSETUP_FILES[i].name);
    if (FFat.exists(path)) continue;  // skip existing files
    File f = FFat.open(path, FILE_WRITE);
    if (!f) { Serial.printf("FFat: failed to create %s\n", path); continue; }
    size_t w = f.write(RIGSETUP_FILES[i].data, RIGSETUP_FILES[i].size);
    f.close();
    if (w != RIGSETUP_FILES[i].size) {
      Serial.printf("FFat: short write on %s — removed\n", path);
      FFat.remove(path);
      continue;
    }
    Serial.printf("FFat: wrote %s (%u bytes)\n", path, (unsigned)RIGSETUP_FILES[i].size);
  }
}

// ============================================================
//  forcePopulateRigsetupFiles()  — overwrite ALL /rigsetup/ files
//  from the firmware-compiled arrays, regardless of whether they
//  already exist.  Called from /api/rigsetup/force-copy.
//  Returns the number of files successfully written.
// ============================================================
int forcePopulateRigsetupFiles() {
  if (!_fatOk) return 0;
  if (!FFat.exists("/rigsetup")) FFat.mkdir("/rigsetup");
  int written = 0;
  for (size_t i = 0; i < RIGSETUP_FILE_COUNT; i++) {
    char path[64];
    snprintf(path, sizeof(path), "/rigsetup/%s", RIGSETUP_FILES[i].name);
    // Remove existing file first so FILE_WRITE starts at byte 0.
    if (FFat.exists(path)) FFat.remove(path);
    File f = FFat.open(path, FILE_WRITE);
    if (!f) { Serial.printf("FFat: force-copy: failed to create %s\n", path); continue; }
    size_t w = f.write(RIGSETUP_FILES[i].data, RIGSETUP_FILES[i].size);
    f.close();
    if (w != RIGSETUP_FILES[i].size) {
      Serial.printf("FFat: force-copy: short write %s — removed\n", path);
      FFat.remove(path);
      continue;
    }
    Serial.printf("FFat: force-copy: wrote %s (%u bytes)\n", path, (unsigned)RIGSETUP_FILES[i].size);
    written++;
  }
  return written;
}

void littleFsBegin() {
  // true = format partition on first use if blank/corrupt
  if (!FFat.begin(true)) {
    Serial.println("ERR: FFat mount failed — check partition scheme");
    _fatOk = false;
    return;
  }
  _fatOk = true;
  Serial.printf("FFat OK  total:%u B  used:%u B\n",
                FFat.totalBytes(), FFat.usedBytes());

  // All config JSON files live under /config/ (mkdir is a no-op if it exists)
  if (!FFat.exists("/config")) FFat.mkdir("/config");

  // Migrate legacy root-level config files to /config/ (one-time, after OTA)
  _fsMigrate("/macros.json", "/config/macros.json");
  _fsMigrate("/menus.json",  "/config/menus.json");

  // RIGSETUP install files
  _ensureRigsetupFiles();
}

// ============================================================
//  GET /api/files — JSON array [{name, size}, …]
//  Files in subdirectories are listed as "dir/name" (e.g.
//  "config/macros.json"), which is also the path used by
//  /files/<name> and /api/files/del.
// ============================================================
static void _fsListDir(const String &dirPath, const String &prefix,
                       String &json, bool &first, int depth) {
  File dir = FFat.open(dirPath);
  if (!dir || !dir.isDirectory()) return;

  File f = dir.openNextFile();
  while (f) {
    String name = f.name();
    int sl = name.lastIndexOf('/');          // some cores return full path
    if (sl >= 0) name = name.substring(sl + 1);

    if (f.isDirectory()) {
      if (depth > 0) {
        String sub = (dirPath == "/" ? "/" : dirPath + "/") + name;
        _fsListDir(sub, prefix + name + "/", json, first, depth - 1);
      }
    } else {
      if (!first) json += ",";
      first = false;
      json += "{\"name\":\"" + _fsJsonEsc(prefix + name) +
              "\",\"size\":"  + String(f.size()) + "}";
    }
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
}

void handleApiFilesList() {
  if (!_fatOk) {
    server.send(503, "application/json", "[]");
    return;
  }

  String json = "[";
  bool   first = true;
  _fsListDir("/", "", json, first, 1);   // root + one level of subdirs
  json += "]";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// ============================================================
//  GET /files/<name>  — stream a file from FFat
//  Called from onNotFound() when URI starts with "/files/".
// ============================================================
static String _mimeType(const String &path) {
  if (path.endsWith(".html") || path.endsWith(".htm")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".txt"))  return "text/plain";
  if (path.endsWith(".sh"))   return "text/x-sh";
  if (path.endsWith(".py"))   return "text/x-python";
  if (path.endsWith(".xml"))  return "text/xml";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".gif"))  return "image/gif";
  if (path.endsWith(".svg"))  return "image/svg+xml";
  if (path.endsWith(".gz"))   return "application/gzip";
  if (path.endsWith(".zip"))  return "application/zip";
  if (path.endsWith(".pdf"))  return "application/pdf";
  return "application/octet-stream";
}

void handleFileServe() {
  if (!_fatOk) {
    server.send(503, "text/plain", "Storage not available");
    return;
  }

  // URI: /files/<name>  →  FAT path: /<name>
  String fsPath = server.uri().substring(6);   // strip "/files", keep '/'
  if (fsPath.isEmpty() || fsPath == "/") {
    server.send(400, "text/plain", "No filename specified");
    return;
  }

  if (!FFat.exists(fsPath)) {
    server.send(404, "text/plain", "File not found: " + fsPath);
    return;
  }

  File f = FFat.open(fsPath, "r");
  if (!f) {
    server.send(500, "text/plain", "Cannot open: " + fsPath);
    return;
  }

  String basename = fsPath;
  int sl = basename.lastIndexOf('/');
  if (sl >= 0) basename = basename.substring(sl + 1);

  server.sendHeader("Content-Disposition",
                    "attachment; filename=\"" + basename + "\"");
  server.sendHeader("Cache-Control", "no-cache");
  server.streamFile(f, _mimeType(fsPath));
  f.close();
}

// ============================================================
//  POST /upload — multipart upload (field name: "file")
// ============================================================
static File   _upFile;
static String _upPath;
static bool   _upOk = false;   // tracks whether write succeeded

void handleFileUploadDone() {
  if (!_upOk) {
    server.send(500, "text/plain", "Upload failed — storage not available or write error");
    return;
  }
  server.send(200, "text/plain", "OK: " + _upPath);
}

void handleFileUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    _upOk = false;

    if (!_fatOk) {
      Serial.println("ERR: FFat not mounted — upload rejected");
      return;
    }

    String name = upload.filename;
    // Keep a single safe directory prefix (e.g. "config/macros.json");
    // anything else is reduced to the base filename.  Replace spaces.
    {
      name.replace('\\', '/');
      while (name.startsWith("/")) name = name.substring(1);
      String dir;
      int sl = name.lastIndexOf('/');
      if (sl >= 0) {
        dir  = name.substring(0, sl);
        name = name.substring(sl + 1);
        if (dir.indexOf('/') >= 0 || dir.indexOf("..") >= 0 || dir == ".")
          dir = "";
      }
      for (unsigned int i = 0; i < name.length(); i++)
        if (name[i] == ' ') name[i] = '_';
      if (name.isEmpty()) name = "upload.bin";
      if (dir.length()) {
        if (!FFat.exists("/" + dir)) FFat.mkdir("/" + dir);
        name = dir + "/" + name;
      }
    }

    _upPath = "/" + name;
    Serial.printf("FAT UPLOAD START: %s\n", _upPath.c_str());

    if (FFat.exists(_upPath)) FFat.remove(_upPath);

    _upFile = FFat.open(_upPath, "w");
    if (!_upFile) {
      Serial.printf("ERR: Cannot open %s for write (disk full?)\n", _upPath.c_str());
    } else {
      _upOk = true;   // provisional — will clear on write error
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_upFile) {
      size_t written = _upFile.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        Serial.printf("ERR: FAT short write (%u/%u)\n", written, upload.currentSize);
        _upOk = false;
      }
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (_upFile) {
      _upFile.close();
      if (_upOk)
        Serial.printf("FAT UPLOAD END: %s  %u bytes\n",
                      _upPath.c_str(), upload.totalSize);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (_upFile) {
      _upFile.close();
      FFat.remove(_upPath);
      _upOk = false;
      Serial.println("FAT UPLOAD ABORTED");
    }
  }
}

// ============================================================
//  POST /api/files/del — delete a file  (field: name)
// ============================================================
void handleFileDelete() {
  if (!_fatOk) {
    server.send(503, "text/plain", "Storage not available");
    return;
  }
  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "ERR: missing 'name' field");
    return;
  }

  String name = server.arg("name");
  name.trim();
  if (name.isEmpty()) {
    server.send(400, "text/plain", "ERR: empty name");
    return;
  }
  if (!name.startsWith("/")) name = "/" + name;

  if (!FFat.exists(name)) {
    server.send(404, "text/plain", "ERR: not found: " + name);
    return;
  }
  if (!FFat.remove(name)) {
    server.send(500, "text/plain", "ERR: remove failed: " + name);
    return;
  }

  server.send(200, "text/plain", "Deleted: " + name);
}
