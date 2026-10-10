// ============================================================
//  littlefs_mgr.ino — FFat initialisation + HTTP file handlers
//
//  Uses ESP32 FFat (FAT over flash) with the predefined partition scheme:
//    Arduino IDE → Tools → Partition Scheme → "16M Flash (2MB APP/12.5MB FATFS)"
//  No custom partitions.csv needed.
//
//  Routes registered in web_server.ino::startWebServer():
//    GET  /api/files      — JSON list of files [{name, size}, …]
//    POST /api/files/del  — delete a file  (field: name)
//    POST /upload         — upload a file  (multipart field: file)
//
//  Files are served via onNotFound() → handleFileServe():
//    GET  /files/<name>   — stream a file from FFat
//
//  `server` is declared in HMS_RigConsole_v4.ino and visible here
//  because Arduino merges all .ino files into one compilation unit.
// ============================================================

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

// ============================================================
//  littleFsBegin()  — called once from setup()
//  (name kept so HMS_RigConsole_v4.ino needs no further change)
// ============================================================
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
}

// ============================================================
//  GET /api/files — JSON array [{name, size}, …]
// ============================================================
void handleApiFilesList() {
  if (!_fatOk) {
    server.send(503, "application/json", "[]");
    return;
  }

  String json = "[";
  bool   first = true;

  File root = FFat.open("/");
  if (root && root.isDirectory()) {
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        if (!first) json += ",";
        first = false;

        String name = f.name();
        if (name.startsWith("/")) name = name.substring(1);

        json += "{\"name\":\"" + _fsJsonEsc(name) +
                "\",\"size\":"  + String(f.size()) + "}";
      }
      f = root.openNextFile();
    }
    root.close();
  }
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
    // Strip directory component and replace spaces
    {
      int sl = name.lastIndexOf('/');
      if (sl >= 0) name = name.substring(sl + 1);
      for (unsigned int i = 0; i < name.length(); i++)
        if (name[i] == ' ') name[i] = '_';
    }
    if (name.isEmpty()) name = "upload.bin";

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
