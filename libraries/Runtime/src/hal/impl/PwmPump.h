#pragma once

#include <stdint.h>

#include "core/Pump.h"

namespace gardener {

// The rig's pump: a JQB2438 through a low side MOSFET, driven by one PWM channel.
//
// This is where the pin is driven and where the two pieces of hardware policy live — the duty cap,
// and the slew rate limit, because a diaphragm pump dislikes a step from 0 to full. The channel's
// frequency and resolution are here too. The pin and the two numbers are the rig's rather than the
// part's, so the composition root passes them in, and this file names nothing above the part.
class PwmPump : public Pump {
 public:
  PwmPump(uint8_t pin, float dutyMaxPercent, float rampPercentPerSec)
      : pin_(pin), dutyMax_(dutyMaxPercent), rampPerSec_(rampPercentPerSec) {}

  bool begin() override;
  void update(uint32_t nowMs) override;

  void setDuty(float percent) override;
  void stop() override { setDuty(0.0f); }
  void halt() override;

  float dutyPercent() const override { return duty_; }

 private:
  void write(float percent);

  const uint8_t pin_;
  const float dutyMax_;
  const float rampPerSec_;
  float duty_ = 0.0f;
  float target_ = 0.0f;
  uint32_t lastUpdateMs_ = 0;
};

}  // namespace gardener
