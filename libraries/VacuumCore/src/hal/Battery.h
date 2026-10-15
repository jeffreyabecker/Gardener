#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "hal/Pins.h"

namespace eggplan {

// 1S Li-ion gauge on a divider into an ADC pin.
//
// This is a readout, not a protection: nothing in the firmware acts on a low reading
// (Q11), the icon and the battery keys are the whole story. percent() is a straight line
// between the configured empty and full volts, which is good enough for a bar and honest
// about being +/-3-5 % at best.
class Battery {
 public:
  bool begin();
  void update(uint32_t nowMs);

  bool present() const { return present_; }
  bool low() const { return present_ && percent_ < cfg::BATTERY_LOW_PERCENT; }
  float volts() const { return volts_; }
  uint8_t percent() const { return percent_; }

 private:
  bool configured_ = false;
  bool present_ = false;
  bool sampled_ = false;
  float volts_ = 0.0f;
  uint8_t percent_ = 0;
  uint32_t lastSampleMs_ = 0;
};

}  // namespace eggplan
