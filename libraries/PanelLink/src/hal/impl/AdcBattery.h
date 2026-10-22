#pragma once

#include <stdint.h>

#include "hal/Battery.h"

namespace gardener {

// The fitted gauge: a 1S Li-ion pack on a divider into an ADC pin. The pin comes from the
// composition root; the divider, the calibration and the volts-to-percent line are here.
class AdcBattery : public Battery {
 public:
  explicit AdcBattery(uint8_t pin) : pin_(pin) {}

  bool begin() override;
  void update(uint32_t nowMs) override;

  bool present() const override { return present_; }
  bool low() const override;
  float volts() const override { return volts_; }
  uint8_t percent() const override { return percent_; }

 private:
  const uint8_t pin_;
  bool configured_ = false;
  bool present_ = false;
  bool sampled_ = false;
  float volts_ = 0.0f;
  uint8_t percent_ = 0;
  uint32_t lastSampleMs_ = 0;
};

}  // namespace gardener
