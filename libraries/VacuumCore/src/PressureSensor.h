#pragma once

#include <Adafruit_MPRLS.h>
#include <Arduino.h>
#include <Wire.h>

#include "VacuumConfig.h"

namespace eggplan {

// Adafruit MPRLS ported pressure sensor (0..25 PSI absolute).
//
// The breakout is an absolute sensor: ambient air sits near 1013 hPa and a
// vacuum reads lower, so "vacuum" values here are reported as the pressure
// deficit relative to a captured ambient reference.
class PressureSensor {
 public:
  bool begin(TwoWire& wire);
  void update(uint32_t nowMs);

  bool present() const { return present_; }
  bool healthy() const { return present_ && errorCount_ == 0; }
  bool readingValid() const { return valid_; }

  float hpa() const { return filteredHpa_; }             // filtered absolute pressure
  float hpaRaw() const { return lastHpa_; }              // last raw sample
  float ambientHpa() const { return ambientHpa_; }
  float vacuumHpa() const;                               // ambient - absolute, >= 0
  float vacuumPercent() const;                           // 0..100 % of one atmosphere

  void setAmbientHpa(float hpa);
  void captureAmbient();

  uint32_t lastSampleMs() const { return lastSampleMs_; }
  uint32_t errorCount() const { return errorCount_; }
  void clearErrors() { errorCount_ = 0; }

  // Convenience read used by scripts and the UI.
  bool readNow(float& outHpa);

 private:
  Adafruit_MPRLS sensor_;
  bool present_ = false;
  bool valid_ = false;
  uint32_t lastPollMs_ = 0;
  uint32_t lastSampleMs_ = 0;
  uint32_t errorCount_ = 0;
  float ambientHpa_ = cfg::DEFAULT_AMBIENT_HPA;
  float lastHpa_ = 0.0f;
  float filteredHpa_ = 0.0f;
  float window_[5] = {0, 0, 0, 0, 0};
  uint8_t windowCount_ = 0;
  uint8_t windowIndex_ = 0;
};

}  // namespace eggplan
