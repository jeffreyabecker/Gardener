#pragma once

#include <Adafruit_MPRLS.h>
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

#include "hal/PressureSensor.h"

namespace gardener {

// The rig's sensor: an Adafruit MPRLS ported pressure breakout on I2C.
//
// This is the only file in the firmware that includes the vendor library, and the only one that
// knows the part reports hPa: the conversion to kPa happens here, at the driver boundary. The part
// is absolute, so begin() takes the ambient reference from its first reading and kpa() reports the
// depth below that. Readings are median filtered over a short window, then EMA filtered.
class MprlsSensor : public PressureSensor {
 public:
  explicit MprlsSensor(TwoWire& wire) : wire_(wire) {}

  bool begin() override;
  void update(uint32_t nowMs) override;

  bool present() const override { return present_; }
  bool readingValid() const override { return valid_; }

  float kpa() const override;

 private:
  bool readNow(float& outKpa);

  TwoWire& wire_;
  Adafruit_MPRLS sensor_;
  bool present_ = false;
  bool valid_ = false;
  uint32_t lastPollMs_ = 0;
  float ambientKpa_ = 0.0f;  // taken once, when the part comes up
  float filteredKpa_ = 0.0f;
  float window_[5] = {0, 0, 0, 0, 0};
  uint8_t windowCount_ = 0;
  uint8_t windowIndex_ = 0;
};

}  // namespace gardener
