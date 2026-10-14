// ============================================================
//  client_names.ino — host names for AP clients
//
//  The DHCP server in ESP-IDF does not keep the hostname a client
//  sends (option 12), so names are looked up after the client has
//  an IP:
//   1. Pi match     — client IP equals one of the Pi's interface IPs
//                     → hostname from STATUS:SYS (no network traffic)
//   2. mDNS reverse — unicast PTR query "d.c.b.a.in-addr.arpa" to
//                     client:5353 from a non-5353 port (RFC 6762 §6.7
//                     legacy unicast; answered by Avahi/Linux, macOS,
//                     iOS, Windows 10+)
//   3. NetBIOS      — NBSTAT query to client:137 (Windows, Samba)
//  Both network queries go out together from one UDP socket and are
//  retried with back-off. Everything is non-blocking.
// ============================================================

#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <esp_wifi_ap_get_sta_list.h>

// ClientName, CN_MAX, CN_NAME_LEN, CN_SRC_* are in config.h (the
// Arduino prototype generator needs the type before this file).
#define CN_LOCAL_PORT 5354      // anything but 5353 → legacy unicast reply
#define CN_SCAN_MS    2000UL

static ClientName _cn[CN_MAX];
static WiFiUDP    _cnUdp;
static bool       _cnUdpOpen = false;

// Back-off between lookup attempts (ms); the last value repeats, so a
// device whose responder starts late is still found.
static const uint32_t _cnRetry[] = { 0, 2000, 5000, 15000, 60000, 300000 };

static ClientName* _cnFind(const uint8_t *mac) {
  for (int i = 0; i < CN_MAX; i++)
    if (_cn[i].used && memcmp(_cn[i].mac, mac, 6) == 0) return &_cn[i];
  return nullptr;
}

// Keep [A-Za-z0-9 ._-], drop a ".local" suffix / trailing dot.
static void _cnSetName(ClientName *c, const char *raw, uint8_t src) {
  char out[CN_NAME_LEN];
  int n = 0;
  for (const char *p = raw; *p && n < CN_NAME_LEN - 1; p++) {
    char ch = *p;
    if (isalnum((unsigned char)ch) || ch == '-' || ch == '_' || ch == '.' || ch == ' ')
      out[n++] = ch;
  }
  out[n] = 0;
  while (n > 0 && (out[n - 1] == '.' || out[n - 1] == ' ')) out[--n] = 0;
  if (n > 6 && strcasecmp(out + n - 6, ".local") == 0) out[n -= 6] = 0;
  if (n == 0) return;
  strlcpy(c->name, out, CN_NAME_LEN);
  c->src = src;
}

// ── Public API ───────────────────────────────────────────────

// Host name for a connected client, or nullptr if not (yet) known.
const char* clientNameFor(const uint8_t *mac) {
  ClientName *c = _cnFind(mac);
  return (c && c->name[0]) ? c->name : nullptr;
}

// "pi" / "mdns" / "netbios" / "" — for /api/apstatus
const char* clientNameSrc(const uint8_t *mac) {
  ClientName *c = _cnFind(mac);
  if (!c || !c->name[0]) return "";
  switch (c->src) {
    case CN_SRC_PI:   return "pi";
    case CN_SRC_MDNS: return "mdns";
    case CN_SRC_NBNS: return "netbios";
  }
  return "";
}

// ── Queries ──────────────────────────────────────────────────

static void _cnSendMdns(ClientName *c) {
  uint8_t pkt[64];
  const uint8_t *b = (const uint8_t*)&c->ip;   // b[0] = first octet
  c->qid = (uint16_t)esp_random();
  int o = 0;
  pkt[o++] = c->qid >> 8; pkt[o++] = c->qid & 0xFF;
  pkt[o++] = 0; pkt[o++] = 0;                  // flags: standard query
  pkt[o++] = 0; pkt[o++] = 1;                  // QDCOUNT 1
  memset(pkt + o, 0, 6); o += 6;               // AN/NS/AR 0
  for (int i = 3; i >= 0; i--) {               // d.c.b.a
    char lab[4];
    int l = snprintf(lab, sizeof(lab), "%u", b[i]);
    pkt[o++] = l; memcpy(pkt + o, lab, l); o += l;
  }
  pkt[o++] = 7; memcpy(pkt + o, "in-addr", 7); o += 7;
  pkt[o++] = 4; memcpy(pkt + o, "arpa", 4);    o += 4;
  pkt[o++] = 0;
  pkt[o++] = 0; pkt[o++] = 12;                 // QTYPE PTR
  pkt[o++] = 0; pkt[o++] = 1;                  // QCLASS IN
  _cnUdp.beginPacket(IPAddress(c->ip), 5353);
  _cnUdp.write(pkt, o);
  _cnUdp.endPacket();
}

static void _cnSendNbns(ClientName *c) {
  uint8_t pkt[50];
  int o = 0;
  uint16_t id = (uint16_t)esp_random();
  pkt[o++] = id >> 8; pkt[o++] = id & 0xFF;
  pkt[o++] = 0; pkt[o++] = 0;                  // flags: query, unicast
  pkt[o++] = 0; pkt[o++] = 1;                  // QDCOUNT 1
  memset(pkt + o, 0, 6); o += 6;
  pkt[o++] = 32;                               // encoded "*" + 15 x NUL
  pkt[o++] = 'C'; pkt[o++] = 'K';
  for (int i = 0; i < 15; i++) { pkt[o++] = 'A'; pkt[o++] = 'A'; }
  pkt[o++] = 0;
  pkt[o++] = 0; pkt[o++] = 0x21;               // NBSTAT
  pkt[o++] = 0; pkt[o++] = 1;                  // IN
  _cnUdp.beginPacket(IPAddress(c->ip), 137);
  _cnUdp.write(pkt, o);
  _cnUdp.endPacket();
}

// ── Response parsing ─────────────────────────────────────────

// Decode a (possibly compressed) DNS name at *off into out as
// dotted text. Advances *off past the name. Returns false on error.
static bool _dnsName(const uint8_t *p, int len, int *off, char *out, int outLen) {
  int  o = *off, n = 0, jumps = 0;
  bool jumped = false;
  if (outLen > 0) out[0] = 0;
  while (o < len) {
    uint8_t l = p[o];
    if (l == 0) { if (!jumped) *off = o + 1; if (out && n < outLen) out[n] = 0; return true; }
    if ((l & 0xC0) == 0xC0) {
      if (o + 1 >= len || ++jumps > 8) return false;
      if (!jumped) *off = o + 2;
      jumped = true;
      o = ((l & 0x3F) << 8) | p[o + 1];
      continue;
    }
    if (o + 1 + l > len) return false;
    if (out) {
      if (n && n < outLen - 1) out[n++] = '.';
      for (int i = 0; i < l && n < outLen - 1; i++) out[n++] = (char)p[o + 1 + i];
      out[n] = 0;
    }
    o += 1 + l;
  }
  return false;
}

static void _cnParseMdns(ClientName *c, const uint8_t *p, int len) {
  if (len < 12) return;
  if ((((uint16_t)p[0] << 8) | p[1]) != c->qid) return;
  if (!(p[2] & 0x80)) return;                  // not a response
  int qd = (p[4] << 8) | p[5], an = (p[6] << 8) | p[7];
  int o = 12;
  for (int i = 0; i < qd; i++) {
    if (!_dnsName(p, len, &o, nullptr, 0)) return;
    o += 4;
  }
  for (int i = 0; i < an && o + 10 <= len; i++) {
    if (!_dnsName(p, len, &o, nullptr, 0) || o + 10 > len) return;
    uint16_t type  = (p[o] << 8) | p[o + 1];
    uint16_t rdlen = (p[o + 8] << 8) | p[o + 9];
    o += 10;
    if (o + rdlen > len) return;
    if (type == 12) {
      char nm[64];
      int ro = o;
      if (_dnsName(p, len, &ro, nm, sizeof(nm))) { _cnSetName(c, nm, CN_SRC_MDNS); return; }
    }
    o += rdlen;
  }
}

static void _cnParseNbns(ClientName *c, const uint8_t *p, int len) {
  if (len < 12 || !(p[2] & 0x80)) return;
  if (((p[6] << 8) | p[7]) < 1) return;
  int o = 12;
  if (!_dnsName(p, len, &o, nullptr, 0) || o + 11 > len) return;
  o += 10;                                     // type, class, ttl, rdlength
  int cnt = p[o++];
  for (int i = 0; i < cnt && o + 18 <= len; i++, o += 18) {
    uint8_t  suffix = p[o + 15];
    uint16_t flags  = (p[o + 16] << 8) | p[o + 17];
    if (suffix == 0x00 && !(flags & 0x8000)) { // workstation name, unique
      char nm[16];
      memcpy(nm, p + o, 15); nm[15] = 0;
      for (int k = 14; k >= 0 && nm[k] == ' '; k--) nm[k] = 0;
      // mDNS names keep their case; prefer them if both arrive
      if (c->src != CN_SRC_MDNS) _cnSetName(c, nm, CN_SRC_NBNS);
      return;
    }
  }
}

static void _cnPollReplies() {
  uint8_t buf[512];
  for (int k = 0; k < 4; k++) {                // a few per loop pass
    int sz = _cnUdp.parsePacket();
    if (sz <= 0) return;
    int len = _cnUdp.read(buf, sizeof(buf));
    uint32_t from = (uint32_t)_cnUdp.remoteIP();
    uint16_t port = _cnUdp.remotePort();
    for (int i = 0; i < CN_MAX; i++) {
      ClientName *c = &_cn[i];
      if (!c->used || c->ip != from || c->src == CN_SRC_PI) continue;
      if (port == 5353)     _cnParseMdns(c, buf, len);
      else if (port == 137) _cnParseNbns(c, buf, len);
      break;
    }
  }
}

// ── Pi match ─────────────────────────────────────────────────

static bool _cnTryPi(ClientName *c) {
  const char *host = piSysGet("host");
  if (!host[0]) return false;
  char ip[16];
  const uint8_t *b = (const uint8_t*)&c->ip;
  snprintf(ip, sizeof(ip), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
  for (int i = 0; i < piIfaceCount; i++)
    if (strcmp(piIfaces[i].ip, ip) == 0) { _cnSetName(c, host, CN_SRC_PI); return true; }
  return false;
}

// ── Service (call from loop) ─────────────────────────────────

void clientNameService() {
  if (!apRunning) {
    if (_cnUdpOpen) { _cnUdp.stop(); _cnUdpOpen = false; }
    for (int i = 0; i < CN_MAX; i++) _cn[i].used = false;
    return;
  }
  if (!_cnUdpOpen) _cnUdpOpen = _cnUdp.begin(CN_LOCAL_PORT);
  if (_cnUdpOpen) _cnPollReplies();

  static uint32_t lastScan = 0;
  uint32_t now = millis();
  if (now - lastScan < CN_SCAN_MS) return;
  lastScan = now;

  static wifi_sta_list_t        wsl;
  static wifi_sta_mac_ip_list_t ipl;
  memset(&wsl, 0, sizeof(wsl));
  memset(&ipl, 0, sizeof(ipl));
  if (esp_wifi_ap_get_sta_list(&wsl) != ESP_OK) return;
  bool haveIp = (esp_wifi_ap_get_sta_list_with_ip(&wsl, &ipl) == ESP_OK);

  // Forget clients that left
  for (int i = 0; i < CN_MAX; i++) {
    if (!_cn[i].used) continue;
    bool present = false;
    for (int s = 0; s < wsl.num; s++)
      if (memcmp(wsl.sta[s].mac, _cn[i].mac, 6) == 0) { present = true; break; }
    if (!present) _cn[i].used = false;
  }

  bool sent = false;                           // max one lookup per scan
  for (int s = 0; s < wsl.num; s++) {
    const uint8_t *mac = wsl.sta[s].mac;
    uint32_t ip = haveIp ? ipl.sta[s].ip.addr : 0;
    ClientName *c = _cnFind(mac);
    if (!c) {
      for (int i = 0; i < CN_MAX; i++) if (!_cn[i].used) { c = &_cn[i]; break; }
      if (!c) continue;
      memset(c, 0, sizeof(*c));
      memcpy(c->mac, mac, 6);
      c->used = true;
    }
    if (c->ip != ip) {                         // new or changed lease → start over
      c->ip = ip; c->name[0] = 0; c->src = CN_SRC_NONE;
      c->tries = 0; c->nextMs = now;
    }
    if (!c->ip) continue;
    // Pi hostname may arrive later than the lease; check every scan
    if (c->src == CN_SRC_NONE || c->src == CN_SRC_PI) {
      if (_cnTryPi(c)) continue;
    }
    if (c->name[0] || sent || !_cnUdpOpen) continue;
    if ((int32_t)(now - c->nextMs) < 0) continue;
    _cnSendMdns(c);
    _cnSendNbns(c);
    sent = true;
    int last = sizeof(_cnRetry) / sizeof(_cnRetry[0]) - 1;
    if (c->tries < last) c->tries++;
    c->nextMs = now + _cnRetry[c->tries];
  }
}
