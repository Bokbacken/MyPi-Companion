// ============================================================
//  msc.ino — USB Mass Storage Class with switchable backends
//
//  One MSC LUN is exposed to the host (Pi).  What it shows
//  depends on the active backend:
//
//    MSC_BACKEND_NONE      no media (host sees an empty drive)
//    MSC_BACKEND_RIGSETUP  256 KB FAT12 volume "RIGSETUP" built in
//                          PSRAM from the files in /rigsetup/ on
//                          FFat (see fat12_builder.ino).  The files
//                          are restored from rigsetup_files.h at
//                          boot by littlefs_mgr.ino if missing.
//    MSC_BACKEND_SD        raw sectors of the SD card (SD Share),
//                          via SD_MMC.readRAW / writeRAW
//
//  On Linux (Pi) RIGSETUP auto-mounts as /media/<user>/RIGSETUP/.
//  The RIGSETUP image lives in PSRAM so the host may write to it
//  freely; it is rebuilt every time the backend is selected.
//
//  Init order in setup():
//    mscBegin()   ← register MSC interface with TinyUSB (+ sdInit())
//    hidBegin()   ← register HID interface + call USB.begin()
//
//  mscBegin() must be called BEFORE hidBegin() so both
//  interfaces are registered before USB.begin() finalises the
//  composite device descriptor.
// ============================================================

#include "USBMSC.h"
#include "fat12_builder.h"

enum MscBackend { MSC_BACKEND_NONE, MSC_BACKEND_RIGSETUP, MSC_BACKEND_SD };

#define MSC_RIGSETUP_BYTES  262144u   // 512 sectors × 512 B
#define MSC_SECTOR          512u

static USBMSC MSC;
static volatile MscBackend msc_backend = MSC_BACKEND_NONE;
static uint8_t  *msc_ram       = nullptr;  // RIGSETUP image in PSRAM
static uint32_t  msc_img_bytes = 0;        // valid size of msc_ram image
static uint32_t  msc_sd_sectors = 0;       // SD card size in 512 B sectors
static uint8_t   msc_bounce[MSC_SECTOR] __attribute__((aligned(4)));

// ── Callbacks (invoked from TinyUSB task) ─────────────────────
//  Named on_msc_* to avoid clashing with the msc_read_cb /
//  msc_write_cb typedefs declared in USBMSC.h.

static int32_t on_msc_read(uint32_t lba, uint32_t offset,
                           void *buffer, uint32_t bufsize) {
  if (msc_backend == MSC_BACKEND_RIGSETUP) {
    uint32_t pos = lba * MSC_SECTOR + offset;
    if (!msc_ram || pos + bufsize > msc_img_bytes) return -1;
    memcpy(buffer, msc_ram + pos, bufsize);
    return (int32_t)bufsize;
  }
  if (msc_backend == MSC_BACKEND_SD) {
    uint8_t *dst  = (uint8_t *)buffer;
    uint32_t left = bufsize;
    while (left) {
      if (lba >= msc_sd_sectors) return -1;
      uint32_t chunk = MSC_SECTOR - offset;
      if (chunk > left) chunk = left;
      if (offset == 0 && chunk == MSC_SECTOR) {
        if (!sdReadSector(dst, lba)) return -1;
      } else {
        if (!sdReadSector(msc_bounce, lba)) return -1;
        memcpy(dst, msc_bounce + offset, chunk);
      }
      dst += chunk; left -= chunk; offset = 0; lba++;
    }
    return (int32_t)bufsize;
  }
  return -1;
}

static int32_t on_msc_write(uint32_t lba, uint32_t offset,
                            uint8_t *buffer, uint32_t bufsize) {
  if (msc_backend == MSC_BACKEND_RIGSETUP) {
    uint32_t pos = lba * MSC_SECTOR + offset;
    if (!msc_ram || pos + bufsize > msc_img_bytes) return -1;
    memcpy(msc_ram + pos, buffer, bufsize);
    return (int32_t)bufsize;
  }
  if (msc_backend == MSC_BACKEND_SD) {
    uint8_t *src  = buffer;
    uint32_t left = bufsize;
    while (left) {
      if (lba >= msc_sd_sectors) return -1;
      uint32_t chunk = MSC_SECTOR - offset;
      if (chunk > left) chunk = left;
      if (offset == 0 && chunk == MSC_SECTOR) {
        if (!sdWriteSector(src, lba)) return -1;
      } else {
        // Partial sector: read-modify-write
        if (!sdReadSector(msc_bounce, lba)) return -1;
        memcpy(msc_bounce + offset, src, chunk);
        if (!sdWriteSector(msc_bounce, lba)) return -1;
      }
      src += chunk; left -= chunk; offset = 0; lba++;
    }
    return (int32_t)bufsize;
  }
  return -1;
}

// ── Graceful eject ────────────────────────────────────────────
//
//  Pulling the media while the host still has the volume mounted
//  loses its dirty page cache ("lost sync page write").  Two paths
//  make removal clean:
//
//  1. ESP-initiated (Stop sharing, switch backend):
//     _mscDetach() sends CMD:MSC_EJECT to the Pi daemon, which runs
//     sync + unmount and answers OK:MSC_EJECT (or ERR:MSC_EJECT:<why>).
//     We pump the serial port until the reply arrives or the timeout
//     expires, and only then hide the media.  Skipped if the daemon
//     is not connected.
//
//  2. Host-initiated (Eject in the Pi file manager / `eject`):
//     the host sends SCSI START STOP UNIT (load_eject, !start); the
//     callback flags it and mscLoop() tears the backend down — the
//     host has already unmounted, so no request is sent.

#define MSC_EJECT_TIMEOUT_MS 10000u

static volatile bool _mscEjectAck    = false;  // set by protocol.ino
static volatile bool _mscHostEjected = false;  // set from TinyUSB task

void mscOnHostEjectReply(bool ok, const String &why) {
  if (!ok) Serial.printf("msc: Pi eject reported error: %s\n", why.c_str());
  _mscEjectAck = true;
}

static bool on_msc_start_stop(uint8_t power_condition, bool start, bool load_eject) {
  (void)power_condition;
  if (load_eject && !start && msc_backend != MSC_BACKEND_NONE) _mscHostEjected = true;
  return true;
}

// Ask the Pi to flush and unmount the volume.  Returns true on reply.
static bool _mscRequestHostEject() {
  if (!piConnected) return false;
  _mscEjectAck = false;
  Serial.println("CMD:MSC_EJECT");
  uint32_t t0 = millis();
  while (millis() - t0 < MSC_EJECT_TIMEOUT_MS) {
    handleSerial();                 // dispatches OK/ERR:MSC_EJECT
    if (_mscEjectAck) return true;
    delay(10);
  }
  Serial.println("msc: no eject reply from Pi — removing media anyway");
  return false;
}

// Called from loop(): handle an eject requested by the host.
void mscLoop() {
  if (!_mscHostEjected) return;
  if (msc_backend == MSC_BACKEND_SD) sdShareStop();   // also remounts SD for the ESP
  else if (msc_backend != MSC_BACKEND_NONE) mscSetBackendNone();
  _mscHostEjected = false;
}

// ── Backend switching ─────────────────────────────────────────

// Hide the media; if another medium was visible, give the host a
// moment to poll TEST UNIT READY and notice the removal before a
// new medium (possibly of a different size) appears.
static void _mscDetach(bool willReattach) {
  bool wasActive = (msc_backend != MSC_BACKEND_NONE);
  if (wasActive && !_mscHostEjected) _mscRequestHostEject();
  MSC.mediaPresent(false);
  msc_backend = MSC_BACKEND_NONE;
  if (wasActive && willReattach) delay(1500);
}

// (int parameter: enum types in signatures break Arduino's prototype generator)
static void _mscAttach(int b, uint32_t sectors) {
  MSC.begin(sectors, MSC_SECTOR);   // updates the reported capacity
  msc_backend = (MscBackend)b;
  MSC.mediaPresent(true);
}

void mscSetBackendNone() {
  _mscDetach(false);
}

// Build RIGSETUP from /rigsetup/ on FFat and present it to the host.
// Returns false (and leaves the media removed) if /rigsetup/ is empty
// or the image cannot be built.
bool mscSetBackendRigsetup() {
  _mscDetach(true);
  if (!msc_ram) msc_ram = (uint8_t *)ps_malloc(MSC_RIGSETUP_BYTES);
  if (!msc_ram) {
    Serial.println("ERR msc: ps_malloc failed");
    return false;
  }
  uint32_t size = 0;
  if (!fat12BuildFromDir("/rigsetup", msc_ram, MSC_RIGSETUP_BYTES, &size) || !size) {
    Serial.println("ERR msc: RIGSETUP build failed — /rigsetup/ empty, too large or unreadable");
    msc_img_bytes = 0;
    return false;
  }
  Serial.printf("msc: RIGSETUP built from /rigsetup (%u B)\n", (unsigned)size);
  msc_img_bytes = size;
  _mscAttach(MSC_BACKEND_RIGSETUP, size / MSC_SECTOR);
  return true;
}

// Expose the (already mounted) SD card as raw block device.
bool mscSetBackendSD() {
  uint32_t sectors = sdSectorCount();
  if (!sectors) {
    Serial.println("ERR msc: SD card not available");
    return false;
  }
  _mscDetach(true);
  msc_sd_sectors = sectors;
  _mscAttach(MSC_BACKEND_SD, sectors);
  return true;
}

bool mscBackendIsRigsetup() { return msc_backend == MSC_BACKEND_RIGSETUP; }
bool mscBackendIsSD()       { return msc_backend == MSC_BACKEND_SD; }

// ── Public init ───────────────────────────────────────────────
//  Call once in setup(), before hidBegin().

void mscBegin() {
  MSC.vendorID("HMS");
  MSC.productID("RigSetup");
  MSC.productRevision("1.0");
  MSC.onRead(on_msc_read);
  MSC.onWrite(on_msc_write);
  MSC.onStartStop(on_msc_start_stop);
  // onFlush() does not exist in ESP32 core 3.x — omitted.
  MSC.mediaPresent(false);   // no media until a backend is selected
  MSC.begin(MSC_RIGSETUP_BYTES / MSC_SECTOR, MSC_SECTOR);  // real size set per backend
  // Note: USB.begin() is called later in hidBegin()

  // SD card init (and SD Share if "enable on boot" is set).
  sdInit();
}

// ── Media insert / eject  (called from hid.ino) ───────────────
//
//  mscInsert()  Rebuilds RIGSETUP so the Pi always sees a clean disk
//               with the current /rigsetup/ files.  Stops SD Share if
//               it was active.  Call at the start of hidInstallOnPi().
//
//  mscEject()   Removes the RIGSETUP media (an active SD Share is left
//               untouched).  Call at every exit point of hidInstallOnPi().

void mscInsert() {
  mscSetBackendRigsetup();
}

void mscEject() {
  if (msc_backend == MSC_BACKEND_RIGSETUP) mscSetBackendNone();
}
