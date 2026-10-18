#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "hal/Battery.h"

namespace gardener {

// The fitted gauge: a 1S Li-ion pack on a divider into an ADC pin.
class AdcBattery : public Battery {
 public:
  bool begin() override;
  void update(uint32_t nowMs) override;

  bool present() const override { return present_; }
  bool low() const override { return present_ && percent_ < cfg::BATTERY_LOW_PERCENT; }
  float volts() const override { return volts_; }
  uint8_t percent() const override { return percent_; }

 private:
  bool configured_ = false;
  bool present_ = false;
  bool sampled_ = false;
  float volts_ = 0.0f;
  uint8_t percent_ = 0;
  uint32_t lastSampleMs_ = 0;
};

}  // namespace gardener
