// ============================================================
//  nvs_macros.ino — Quick Button NVS storage
//
//  The macro pool (/config/macros.json) is stored on FFat and managed
//  entirely client-side.  The ESP only needs to remember which
//  macro ID is assigned to each of the 25 quick-button slots.
//
//  Preferences namespace : "qbtns"
//  Keys                  : "q0" … "q24"  (int32)
//  Value                 : macro ID  or  -1 (empty slot)
// ============================================================

// Global array — one entry per quick-button slot (25 total).
int32_t quickBtnIds[25];

// ------------------------------------------------------------
//  loadMacros() — stub kept for compatibility with setup()
//  The actual macro pool lives in /config/macros.json on FFat and is
//  loaded by the browser, not by the ESP firmware.
// ------------------------------------------------------------
void loadMacros() {
  // Nothing to do — macro pool is client-side (FFat).
}

// ------------------------------------------------------------
//  loadQuickBtns() — read all 25 slots from NVS at boot
// ------------------------------------------------------------
void loadQuickBtns() {
  Preferences prefs;
  prefs.begin("qbtns", true);   // read-only
  char key[5];
  for (int i = 0; i < 25; i++) {
    snprintf(key, sizeof(key), "q%d", i);
    quickBtnIds[i] = prefs.getInt(key, -1);
  }
  prefs.end();
}

// ------------------------------------------------------------
//  saveQuickBtnSlot() — persist one slot to NVS
//  Called from the HTTP handler after the browser POSTs a change.
// ------------------------------------------------------------
void saveQuickBtnSlot(int slot, int32_t macroId) {
  if (slot < 0 || slot >= 25) return;
  Preferences prefs;
  prefs.begin("qbtns", false);  // read-write
  char key[5];
  snprintf(key, sizeof(key), "q%d", slot);
  prefs.putInt(key, macroId);
  prefs.end();
  // Mirror to in-memory array
  quickBtnIds[slot] = macroId;
}
