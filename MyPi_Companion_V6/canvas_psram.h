#pragma once
// ============================================================
//  canvas_psram.h — Arduino_Canvas subclass that places its
//  RGB565 framebuffer in OPI-PSRAM (ps_malloc) instead of
//  internal SRAM.
//
//  Framebuffer size: 172 × 320 × 2 = 107 520 bytes (≈ 105 KB)
//
//  Usage — drop-in replacement for Arduino_Canvas:
//    Arduino_Canvas_PSRAM *gfx =
//        new Arduino_Canvas_PSRAM(172, 320, panel, 0, 0, 0);
//    gfx->begin();
//    if (gfx->_inPSRAM) Serial.println("Framebuffer in PSRAM");
//
//  If psramFound() returns false the buffer remains in SRAM
//  and _inPSRAM stays false.  begin() still returns true, so
//  calling code needs no special handling.
// ============================================================
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

class Arduino_Canvas_PSRAM : public Arduino_Canvas {
public:
  bool _inPSRAM = false;   // true after successful PSRAM migration

  Arduino_Canvas_PSRAM(int16_t w, int16_t h, Arduino_G *output,
                       int16_t output_x = 0, int16_t output_y = 0,
                       uint8_t rotation = 0)
      : Arduino_Canvas(w, h, output, output_x, output_y, rotation)
  {}

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    // Base class allocates _framebuffer with malloc() in internal SRAM
    if (!Arduino_Canvas::begin(speed)) return false;

    if (!psramFound()) return true;   // no PSRAM — stay in SRAM silently

    size_t sz = (size_t)_width * _height * 2;   // RGB565 = 2 bytes per pixel
    uint16_t *pb = (uint16_t *)ps_malloc(sz);
    if (!pb) {
      Serial.println("WARN: ps_malloc for framebuffer failed — staying in SRAM");
      return true;
    }

    // Copy blank buffer, free old allocation, redirect pointer
    memcpy(pb, _framebuffer, sz);
    free(_framebuffer);
    _framebuffer = pb;
    _inPSRAM     = true;

    Serial.printf("INFO: Canvas framebuffer in PSRAM (%u bytes)\r\n", (unsigned)sz);
    return true;
  }
};
