#pragma once

#include <stdint.h>

#include "hal/PressureSensor.h"

namespace gardener {

// The rig's sensor: a CFSensor XGZP6847A100KPGN33, a -100..0 kPa gauge part with an amplified
// analog output. Its reading is a voltage, so the ADC pin it sits on is the whole interface.
//
// The part's facts are here rather than in the board's pin map: at 3.3 V its nominal output is
// 0.2 V at -100 kPa and 2.7 V at 0 kPa, or 0.025 V per kPa. begin() records the output at ambient
// and pressureKpa() measures below it, which cancels the part's own zero offset. Samples are averaged,
// then EMA filtered.
class XgzpSensor : public PressureSensor {
 public:
  explicit XgzpSensor(uint8_t pin) : pin_(pin) {}

  bool begin() override;
  void update(uint32_t nowMs) override;

  bool present() const override { return present_; }
  bool readingValid() const override { return valid_; }

  float pressureKpa() const override;

 private:
  bool sample(float& outVolts);
  void applySample(float volts);

  const uint8_t pin_;
  bool configured_ = false;
  bool present_ = false;
  bool valid_ = false;
  uint32_t lastPollMs_ = 0;
  float ambientVolts_ = 0.0f;
  float filteredVolts_ = 0.0f;
};

}  // namespace gardener
