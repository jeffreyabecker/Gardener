#pragma once

#include <Adafruit_MPRLS.h>
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

#include "core/Config.h"
#include "hal/Pins.h"

namespace gardener {

// Adafruit MPRLS ported pressure sensor (0..25 PSI absolute), on the I2C bus.
//
// Reports kPa: the library's hPa is converted once, here, at the driver boundary.
// The part is absolute, so ambient air sits near 101.3 kPa and a vacuum reads lower;
// "vacuum" values are the deficit against a captured ambient reference.
class PressureSensor {
 public:
  bool begin(TwoWire& wire);
  void update(uint32_t nowMs);

  bool present() const { return present_; }
  bool readingValid() const { return valid_; }

  float kpa() const { return filteredKpa_; }
  float ambientKpa() const { return ambientKpa_; }
  float vacuumKpa() const;
  float vacuumPercent() const;

  // False when the value is outside what the part can measure; the reference is then
  // left alone, so a caller that sends nonsense learns about it instead of silently
  // getting the old ambient back.
  bool setAmbientKpa(float kpa);
  void captureAmbient();

 private:
  bool readNow(float& outKpa);

  Adafruit_MPRLS sensor_;
  bool present_ = false;
  bool valid_ = false;
  uint32_t lastPollMs_ = 0;
  float ambientKpa_ = 0.0f;
  float filteredKpa_ = 0.0f;
  float window_[5] = {0, 0, 0, 0, 0};
  uint8_t windowCount_ = 0;
  uint8_t windowIndex_ = 0;
};

}  // namespace gardener
