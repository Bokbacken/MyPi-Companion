#pragma once
// ============================================================
//  touch_axs5106l.h — Driver for AXS5106L capacitive touch
//
//  Ported directly from Waveshare's esp_lcd_touch_axs5106l.cpp
//  (uploaded source). Key protocol:
//
//    1. RST LOW 200 ms → HIGH 300 ms
//    2. FALLING-edge ISR sets _axs_int_flag = true
//    3. axs5106l_read() only runs when flag is true, then clears it
//    4. I2C: write reg 0x01 (STOP), then requestFrom 14 bytes
//    5. Coordinates: data[1]=count, data[2..5]=x_hi,x_lo,y_hi,y_lo
//
//  I2C address: 0x63
//  Touch data register: 0x01
//  ID register: 0x08
// ============================================================

#include <Wire.h>

#define AXS_ADDR        0x63
#define AXS_REG_TOUCH   0x01
#define AXS_REG_ID      0x08

// ---- Debug toggle ----------------------------------------
// #define TOUCH_DEBUG     // uncomment to re-enable serial debug

struct TouchPoint {
  int16_t x;
  int16_t y;
};

static volatile bool     _axs_int_flag  = false;
static volatile uint32_t _axs_isr_count = 0;   // total ISR fires (diagnostic)
static bool              _axs_ready     = false;
static int               _axs_int_pin   = -1;

// ----------------------------------------------------------
//  Gesture caching
//  The AXS5106L ISR fires only on FALLING edges (touch-down
//  + position updates), NOT continuously while held.
//  Without caching every loop iteration without a new ISR
//  would return false, making handleTouch() classify all
//  gestures as zero-displacement taps.
//
//  Strategy: cache the last valid touch point + timestamp.
//  Return "still pressed" with the cached position until
//  AXS_LIFT_TIMEOUT_MS passes with no new interrupt.
// ----------------------------------------------------------
static bool       _axs_pressed_state = false;
static TouchPoint _axs_cached_tp     = {0, 0};
static uint32_t   _axs_last_touch_ms = 0;
#define AXS_LIFT_TIMEOUT_MS  120   // ms without new ISR → lift

// Interrupt service routine — called on FALLING edge of INT pin
static void IRAM_ATTR _axs_isr() {
  _axs_int_flag = true;
  _axs_isr_count++;
}

// ----------------------------------------------------------
//  axs5106l_init()
//  Mirrors bsp_touch_init() from Waveshare BSP exactly.
//  Returns true if chip acknowledged on I2C bus.
// ----------------------------------------------------------
bool axs5106l_init(int sda, int scl, int rst_pin, int int_pin) {
  _axs_int_pin = int_pin;

  Wire.begin(sda, scl);
  Wire.setClock(400000);

  // Hardware reset: exactly as BSP (LOW 200 ms, HIGH 300 ms)
  if (rst_pin >= 0) {
    pinMode(rst_pin, OUTPUT);
    digitalWrite(rst_pin, LOW);
    delay(200);
    digitalWrite(rst_pin, HIGH);
    delay(300);
  }

  // Attach FALLING-edge interrupt (same as BSP attachInterrupt)
  pinMode(int_pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(int_pin), _axs_isr, FALLING);

  // Read chip ID from reg 0x08 (same as BSP post-init verify)
  uint8_t id[3] = {0};
  Wire.beginTransmission(AXS_ADDR);
  Wire.write(AXS_REG_ID);
  Wire.endTransmission();          // STOP — same as BSP
  Wire.requestFrom((uint8_t)AXS_ADDR, (uint8_t)3);
  for (int i = 0; i < 3 && Wire.available(); i++) id[i] = Wire.read();

  // Probe: ACK from 0x63 confirms chip is alive
  Wire.beginTransmission(AXS_ADDR);
  uint8_t err = Wire.endTransmission();
  _axs_ready = (err == 0);

#ifdef TOUCH_DEBUG
  Serial.print("DBG TOUCH init: probe 0x63 err="); Serial.println(err);
  Serial.print("DBG TOUCH ID reg 0x08: ");
  Serial.print(id[0], HEX); Serial.print(" ");
  Serial.print(id[1], HEX); Serial.print(" ");
  Serial.println(id[2], HEX);
  Serial.print("DBG TOUCH RST="); Serial.print(rst_pin);
  Serial.print("  INT="); Serial.print(int_pin);
  Serial.print("  SDA="); Serial.print(sda);
  Serial.print("  SCL="); Serial.println(scl);
#endif

  return _axs_ready;
}

// ----------------------------------------------------------
//  axs5106l_available()
//  Returns true if a FALLING-edge interrupt has been registered.
//  Non-destructive peek — flag stays set until axs5106l_read().
// ----------------------------------------------------------
bool axs5106l_available() {
  return _axs_ready && _axs_int_flag;
}

// ----------------------------------------------------------
//  axs5106l_read()
//  Returns true while a finger is on the screen, false when lifted.
//
//  Caching strategy (needed because ISR fires only on edges):
//    • ISR fired → read chip → update cache → return true
//    • ISR not fired, cache valid, within timeout → return cached tp
//    • ISR not fired, timeout expired → clear cache → return false
//
//  Coordinate mapping (BSP rotation 0 / default case):
//    tp.x = 171 - raw_x   (flip X — chip origin is at right edge)
//    tp.y = raw_y          (Y unchanged — chip origin already at top)
// ----------------------------------------------------------
bool axs5106l_read(TouchPoint &tp) {
  if (!_axs_ready) return false;

  uint32_t now = (uint32_t)millis();

  if (_axs_int_flag) {
    _axs_int_flag = false;  // clear before I2C so re-entry is safe

    uint8_t data[14] = {0};
    Wire.beginTransmission(AXS_ADDR);
    Wire.write(AXS_REG_TOUCH);
    if (Wire.endTransmission() != 0) {
      // I2C error — keep cached state if recent
      if (_axs_pressed_state &&
          (now - _axs_last_touch_ms) < AXS_LIFT_TIMEOUT_MS) {
        tp = _axs_cached_tp;
        return true;
      }
      _axs_pressed_state = false;
      return false;
    }

    uint8_t got = Wire.requestFrom((uint8_t)AXS_ADDR, (uint8_t)14);
    if (got < 6) {
      _axs_pressed_state = false;
      return false;
    }
    for (int i = 0; i < got && i < 14; i++) data[i] = Wire.read();

#ifdef TOUCH_DEBUG
    Serial.print("DBG TOUCH raw got="); Serial.print(got);
    Serial.print("  [");
    for (int i = 0; i < got && i < 14; i++) {
      Serial.print(data[i], HEX); Serial.print(" ");
    }
    Serial.println("]");
#endif

    uint8_t count = data[1];
    if (count == 0) {
      // Chip reported touch-up explicitly
      _axs_pressed_state = false;
      return false;
    }

    // BSP coordinate extraction for first touch point
    int16_t raw_x = (int16_t)(((data[2] & 0x0F) << 8) | data[3]);
    int16_t raw_y = (int16_t)(((data[4] & 0x0F) << 8) | data[5]);

    // BSP rotation 0 (default): flip X only, Y is already correct
    tp.x = 171 - raw_x;
    tp.y = raw_y;

#ifdef TOUCH_DEBUG
    Serial.print("DBG TOUCH count="); Serial.print(count);
    Serial.print("  raw="); Serial.print(raw_x); Serial.print(","); Serial.print(raw_y);
    Serial.print("  mapped="); Serial.print(tp.x); Serial.print(","); Serial.println(tp.y);
#endif

    _axs_cached_tp     = tp;
    _axs_pressed_state = true;
    _axs_last_touch_ms = now;
    return true;
  }

  // No new interrupt — return cached state if within lift timeout
  if (_axs_pressed_state) {
    if ((now - _axs_last_touch_ms) < AXS_LIFT_TIMEOUT_MS) {
      tp = _axs_cached_tp;
      return true;
    }
    _axs_pressed_state = false;  // timeout → lift
  }

  return false;
}
