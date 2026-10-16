#pragma once

#include <Adafruit_MPRLS.h>
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

#include "core/Config.h"
#include "hal/PressureSensor.h"

namespace gardener {

// The rig's sensor: an Adafruit MPRLS ported pressure breakout on I2C.
//
// This is the only file in the firmware that includes the vendor library, and the only one that
// knows the part reports hPa: the conversion to kPa happens here, at the driver boundary. The part
// is absolute, so ambient air sits near 101.3 kPa and a vacuum reads lower, and the deficit against
// a captured reference is what the plant regulates on. Readings are median filtered over a short
// window, then EMA filtered.
class MprlsSensor : public PressureSensor {
 public:
  explicit MprlsSensor(TwoWire& wire) : wire_(wire) {}

  bool begin() override;
  void update(uint32_t nowMs) override;

  bool present() const override { return present_; }
  bool readingValid() const override { return valid_; }

  float kpa() const override { return filteredKpa_; }
  float ambientKpa() const override { return ambientKpa_; }
  float vacuumKpa() const override;
  float vacuumPercent() const override;

  void captureAmbient() override;

 private:
  bool readNow(float& outKpa);

  // False when the value is outside what the part can measure; the reference is then left alone,
  // so a capture taken while the chamber was already pumped down is refused rather than adopted.
  // Only captureAmbient() calls it, so the reference can be taken but never dialled in.
  bool setAmbientKpa(float kpa);

  TwoWire& wire_;
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
