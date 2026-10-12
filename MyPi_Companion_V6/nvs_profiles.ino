// ============================================================
//  nvs_profiles.ino — NVS profile storage
//  Preferences namespace: "rigcfg"
//  4 slots, each stored with keys: s0ssid, s0pass, s0dhcp, ...
// ============================================================

void loadNvsProfiles() {
  Preferences prefs;
  prefs.begin("rigcfg", true);   // read-only
  for (int i = 0; i < MAX_NVS_SLOTS; i++) {
    char key[12];
    memset(&nvsProfiles[i], 0, sizeof(NvsProfile));
    snprintf(key, sizeof(key), "s%dv", i);
    nvsProfiles[i].valid = prefs.getBool(key, false);
    if (!nvsProfiles[i].valid) continue;
    snprintf(key, sizeof(key), "s%dssid", i);
    strlcpy(nvsProfiles[i].ssid, prefs.getString(key, "").c_str(), 33);
    snprintf(key, sizeof(key), "s%dpass", i);
    strlcpy(nvsProfiles[i].pass, prefs.getString(key, "").c_str(), 65);
    snprintf(key, sizeof(key), "s%ddhcp", i);
    nvsProfiles[i].dhcp = prefs.getBool(key, true);
    snprintf(key, sizeof(key), "s%dip",   i);
    strlcpy(nvsProfiles[i].ip,  prefs.getString(key, "").c_str(), 16);
    snprintf(key, sizeof(key), "s%dsn",   i);
    strlcpy(nvsProfiles[i].sn,  prefs.getString(key, "255.255.255.0").c_str(), 16);
    snprintf(key, sizeof(key), "s%dgw",   i);
    strlcpy(nvsProfiles[i].gw,  prefs.getString(key, "").c_str(), 16);
    snprintf(key, sizeof(key), "s%ddns",  i);
    strlcpy(nvsProfiles[i].dns, prefs.getString(key, "").c_str(), 16);
  }
  prefs.end();
}

void saveNvsSlot(int slot, const char* ssid, const char* pass, bool dhcp,
                 const char* ip, const char* sn, const char* gw, const char* dns) {
  if (slot < 0 || slot >= MAX_NVS_SLOTS) return;
  Preferences prefs;
  prefs.begin("rigcfg", false);  // read-write
  char key[12];
  snprintf(key, sizeof(key), "s%dssid", slot); prefs.putString(key, ssid);
  snprintf(key, sizeof(key), "s%dpass", slot); prefs.putString(key, pass);
  snprintf(key, sizeof(key), "s%ddhcp", slot); prefs.putBool(key,   dhcp);
  snprintf(key, sizeof(key), "s%dip",   slot); prefs.putString(key, ip);
  snprintf(key, sizeof(key), "s%dsn",   slot); prefs.putString(key, sn);
  snprintf(key, sizeof(key), "s%dgw",   slot); prefs.putString(key, gw);
  snprintf(key, sizeof(key), "s%ddns",  slot); prefs.putString(key, dns);
  snprintf(key, sizeof(key), "s%dv",    slot); prefs.putBool(key,   true);
  prefs.end();
  // Mirror to in-memory struct
  strlcpy(nvsProfiles[slot].ssid, ssid, 33);
  strlcpy(nvsProfiles[slot].pass, pass, 65);
  nvsProfiles[slot].dhcp  = dhcp;
  strlcpy(nvsProfiles[slot].ip,  ip,  16);
  strlcpy(nvsProfiles[slot].sn,  sn,  16);
  strlcpy(nvsProfiles[slot].gw,  gw,  16);
  strlcpy(nvsProfiles[slot].dns, dns, 16);
  nvsProfiles[slot].valid = true;
}

void deleteNvsSlot(int slot) {
  if (slot < 0 || slot >= MAX_NVS_SLOTS) return;
  Preferences prefs;
  prefs.begin("rigcfg", false);
  char key[12];
  const char* fields[] = {"ssid","pass","dhcp","ip","sn","gw","dns","v"};
  for (int f = 0; f < 8; f++) {
    snprintf(key, sizeof(key), "s%d%s", slot, fields[f]);
    prefs.remove(key);
  }
  prefs.end();
  memset(&nvsProfiles[slot], 0, sizeof(NvsProfile));
  nvsProfiles[slot].valid = false;
}
