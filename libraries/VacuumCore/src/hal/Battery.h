#pragma once

#include <stdint.h>

namespace eggplan {

struct BatteryConfig {
  int adcPin = -1;
  float dividerRatio = 2.0f;      // 100k / 100k across a 1S Li-ion pack
  float emptyVolts = 3.2f;
  float fullVolts = 4.2f;
  float calibration = 1.0f;       // trim against a meter
  float presentMinVolts = 1.0f;   // below this, assume no pack is fitted
  uint8_t lowPercent = 20;
  uint8_t samples = 8;            // a single ADC sample on this part is noisy
  uint32_t periodMs = 500;
};

// 1S Li-ion gauge on a divider into an ADC pin.
//
// This is a readout, not a protection: nothing in the firmware acts on a low reading
// (Q11), the icon and the battery keys are the whole story. percent() is a straight
// line between the configured empty and full volts, which is good enough for a bar and
// honest about being +/-3-5 % at best.
class Battery {
 public:
  explicit Battery(const BatteryConfig& config) : config_(config) {}

  bool begin();
  void update(uint32_t nowMs);

  bool present() const { return present_; }
  bool low() const { return present_ && percent_ < config_.lowPercent; }
  float volts() const { return volts_; }
  uint8_t percent() const { return percent_; }

 private:
  BatteryConfig config_;
  bool configured_ = false;
  bool present_ = false;
  bool sampled_ = false;
  float volts_ = 0.0f;
  uint8_t percent_ = 0;
  uint32_t lastSampleMs_ = 0;
};

}  // namespace eggplan
