#pragma once

#include <Adafruit_MPRLS.h>
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

#include "hal/Pins.h"

namespace eggplan {

struct PressureSensorConfig {
  uint8_t address = pins::MPRLS_I2C_ADDR;
  uint32_t pollMs = pins::MPRLS_POLL_MS;
  float minKpa = pins::MPRLS_MIN_KPA;
  float maxKpa = pins::MPRLS_MAX_KPA;
};

// Adafruit MPRLS ported pressure sensor (0..25 PSI absolute).
//
// Reports kPa: the library's hPa is converted once, here, at the driver boundary.
// The part is absolute, so ambient air sits near 101.3 kPa and a vacuum reads lower;
// "vacuum" values are the deficit against a captured ambient reference.
class PressureSensor {
 public:
  explicit PressureSensor(const PressureSensorConfig& config = PressureSensorConfig())
      : config_(config) {}

  bool begin(TwoWire& wire);
  void update(uint32_t nowMs);

  bool present() const { return present_; }
  bool healthy() const { return present_ && errorCount_ == 0; }
  bool readingValid() const { return valid_; }

  float kpa() const { return filteredKpa_; }
  float kpaRaw() const { return lastKpa_; }
  float ambientKpa() const { return ambientKpa_; }
  float vacuumKpa() const;
  float vacuumPercent() const;

  void setAmbientKpa(float kpa);
  void captureAmbient();

  uint32_t lastSampleMs() const { return lastSampleMs_; }
  uint32_t errorCount() const { return errorCount_; }
  void clearErrors() { errorCount_ = 0; }

  bool readNow(float& outKpa);

 private:
  PressureSensorConfig config_;
  Adafruit_MPRLS sensor_;
  bool present_ = false;
  bool valid_ = false;
  uint32_t lastPollMs_ = 0;
  uint32_t lastSampleMs_ = 0;
  uint32_t errorCount_ = 0;
  float ambientKpa_ = 0.0f;
  float lastKpa_ = 0.0f;
  float filteredKpa_ = 0.0f;
  float window_[5] = {0, 0, 0, 0, 0};
  uint8_t windowCount_ = 0;
  uint8_t windowIndex_ = 0;
};

}  // namespace eggplan
