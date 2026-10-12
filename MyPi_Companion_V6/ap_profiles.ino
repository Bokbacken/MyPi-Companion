// ============================================================
//  ap_profiles.ino — 4 configurable AP profiles (NVS namespace "apcfg")
//
//  Keys per slot N (0-3): aNv, aNname, aNssid, aNpass, aNsub, aNnat
//  Defaults are written on first boot (namespace has no "a0v" key).
// ============================================================

#include <Preferences.h>

// Populate dynApIp/dynApGw/dynApSn from a slot's subnet index.
void apComputeNetwork(int slot) {
  if (slot < 0 || slot >= MAX_AP_PROFILES) slot = 0;
  uint8_t sub = apProfiles[slot].subnet;
  if (sub >= MAX_AP_PROFILES) sub = 0;
  const uint8_t* s = AP_SUBNET_DEFS[sub];
  dynApIp = IPAddress(s[0], s[1], s[2], 1);
  dynApGw = IPAddress(s[0], s[1], s[2], 1);
  dynApSn = IPAddress(255, 255, 255, 0);
}

static void _apWriteDefaults() {
  static const char*   defNames[MAX_AP_PROFILES] = {"RigSetup", "Hotel", "Lab", "Custom"};
  static const char*   defSsids[MAX_AP_PROFILES] = {"RigSetup", "RigSetup-Hotel", "RigSetup-Lab", "RigSetup-4"};
  static const uint8_t defSubs[MAX_AP_PROFILES]  = {0, 1, 2, 3};
  static const bool    defNat[MAX_AP_PROFILES]   = {false, true, false, false};
  Preferences w;
  w.begin("apcfg", false);
  for (int i = 0; i < MAX_AP_PROFILES; i++) {
    char key[12];
    snprintf(key, sizeof(key), "a%dv", i);    w.putBool(key, true);
    snprintf(key, sizeof(key), "a%dname", i); w.putString(key, defNames[i]);
    snprintf(key, sizeof(key), "a%dssid", i); w.putString(key, defSsids[i]);
    snprintf(key, sizeof(key), "a%dpass", i); w.putString(key, AP_PASS_STR);
    snprintf(key, sizeof(key), "a%dsub", i);  w.putUChar(key, defSubs[i]);
    snprintf(key, sizeof(key), "a%dnat", i);  w.putBool(key, defNat[i]);
  }
  w.end();
  Serial.println("[AP] Wrote default AP profiles to NVS");
}

void loadApProfiles() {
  Preferences prefs;
  // Read-only begin() fails if the namespace does not exist yet
  bool fresh = !prefs.begin("apcfg", true) || !prefs.isKey("a0v");
  prefs.end();
  if (fresh) _apWriteDefaults();

  Preferences r;
  r.begin("apcfg", true);
  for (int i = 0; i < MAX_AP_PROFILES; i++) {
    char key[12];
    snprintf(key, sizeof(key), "a%dv", i);
    apProfiles[i].valid = r.getBool(key, false);
    snprintf(key, sizeof(key), "a%dname", i);
    strlcpy(apProfiles[i].name, r.getString(key, "").c_str(), sizeof(apProfiles[i].name));
    snprintf(key, sizeof(key), "a%dssid", i);
    strlcpy(apProfiles[i].ssid, r.getString(key, "").c_str(), sizeof(apProfiles[i].ssid));
    snprintf(key, sizeof(key), "a%dpass", i);
    strlcpy(apProfiles[i].pass, r.getString(key, "").c_str(), sizeof(apProfiles[i].pass));
    snprintf(key, sizeof(key), "a%dsub", i);
    apProfiles[i].subnet = r.getUChar(key, 0);
    if (apProfiles[i].subnet >= MAX_AP_PROFILES) apProfiles[i].subnet = 0;
    snprintf(key, sizeof(key), "a%dnat", i);
    apProfiles[i].nat = r.getBool(key, false);

    // Never leave a slot unusable: fall back to safe values
    if (!apProfiles[i].name[0]) snprintf(apProfiles[i].name, sizeof(apProfiles[i].name), "AP %d", i + 1);
    if (!apProfiles[i].ssid[0]) snprintf(apProfiles[i].ssid, sizeof(apProfiles[i].ssid), "RigSetup-%d", i + 1);
    if (strlen(apProfiles[i].pass) < 8) strlcpy(apProfiles[i].pass, AP_PASS_STR, sizeof(apProfiles[i].pass));
  }
  r.end();
}

void saveApProfile(int slot) {
  if (slot < 0 || slot >= MAX_AP_PROFILES) return;
  Preferences prefs;
  prefs.begin("apcfg", false);
  char key[12];
  snprintf(key, sizeof(key), "a%dv", slot);    prefs.putBool(key, true);
  snprintf(key, sizeof(key), "a%dname", slot); prefs.putString(key, apProfiles[slot].name);
  snprintf(key, sizeof(key), "a%dssid", slot); prefs.putString(key, apProfiles[slot].ssid);
  snprintf(key, sizeof(key), "a%dpass", slot); prefs.putString(key, apProfiles[slot].pass);
  snprintf(key, sizeof(key), "a%dsub", slot);  prefs.putUChar(key, apProfiles[slot].subnet);
  snprintf(key, sizeof(key), "a%dnat", slot);  prefs.putBool(key, apProfiles[slot].nat);
  prefs.end();
  apProfiles[slot].valid = true;
}
