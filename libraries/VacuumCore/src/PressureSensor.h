#pragma once

#include <Adafruit_MPRLS.h>
#include <Arduino.h>
#include <Wire.h>

#include "VacuumConfig.h"

namespace eggplan {

// Adafruit MPRLS ported pressure sensor (0..25 PSI absolute).
//
// The breakout is an absolute sensor and the driver reports kPa: ambient air
// sits near 101.3 kPa and a vacuum reads lower, so "vacuum" values here are the
// pressure deficit relative to a captured ambient reference.
class PressureSensor {
 public:
  bool begin(TwoWire& wire);
  void update(uint32_t nowMs);

  bool present() const { return present_; }
  bool healthy() const { return present_ && errorCount_ == 0; }
  bool readingValid() const { return valid_; }

  float kpa() const { return filteredKpa_; }             // filtered absolute pressure
  float kpaRaw() const { return lastKpa_; }              // last raw sample
  float ambientKpa() const { return ambientKpa_; }
  float vacuumKpa() const;                               // ambient - absolute, >= 0
  float vacuumPercent() const;                           // 0..100 % of one atmosphere

  void setAmbientKpa(float kpa);
  void captureAmbient();

  uint32_t lastSampleMs() const { return lastSampleMs_; }
  uint32_t errorCount() const { return errorCount_; }
  void clearErrors() { errorCount_ = 0; }

  // Convenience read used by scripts and the UI.
  bool readNow(float& outKpa);

 private:
  Adafruit_MPRLS sensor_;
  bool present_ = false;
  bool valid_ = false;
  uint32_t lastPollMs_ = 0;
  uint32_t lastSampleMs_ = 0;
  uint32_t errorCount_ = 0;
  float ambientKpa_ = cfg::DEFAULT_AMBIENT_KPA;
  float lastKpa_ = 0.0f;
  float filteredKpa_ = 0.0f;
  float window_[5] = {0, 0, 0, 0, 0};
  uint8_t windowCount_ = 0;
  uint8_t windowIndex_ = 0;
};

}  // namespace eggplan
