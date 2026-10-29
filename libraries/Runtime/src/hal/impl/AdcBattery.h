#pragma once

#include <stdint.h>

#include "core/Battery.h"

namespace gardener {

// The fitted gauge: a 1S Li-ion pack on a divider into an ADC pin. The pin and the board's divider
// come from the composition root; the calibration and the volts-to-percent line are here.
class AdcBattery : public Battery {
 public:
  AdcBattery(uint8_t pin, float divider) : pin_(pin), divider_(divider) {}

  bool begin() override;
  void update(uint32_t nowMs) override;

  bool present() const override { return present_; }
  bool low() const override;
  float volts() const override { return volts_; }
  uint8_t percent() const override { return percent_; }

 private:
  const uint8_t pin_;
  const float divider_;
  bool configured_ = false;
  bool present_ = false;
  bool sampled_ = false;
  float volts_ = 0.0f;
  uint8_t percent_ = 0;
  uint32_t lastSampleMs_ = 0;
};

}  // namespace gardener
