#pragma once

#include <stdint.h>

#include "hal/Pump.h"

namespace gardener {

// The rig's pump: a JQB2438 through a low side MOSFET, driven by one PWM channel.
//
// This is where the pin is driven and where the two pieces of hardware policy live — the duty cap,
// and the slew rate limit, because a diaphragm pump dislikes a step from 0 to full. The channel's
// frequency and resolution are here too; a board variant edits this file for how the pump is
// driven and app/Build.cpp for which pin drives it, and nothing above it has to know.
class PwmPump : public Pump {
 public:
  explicit PwmPump(uint8_t pin) : pin_(pin) {}

  bool begin() override;
  void update(uint32_t nowMs) override;

  void setDuty(float percent) override;
  void stop() override { setDuty(0.0f); }
  void halt() override;

  float dutyPercent() const override { return duty_; }

 private:
  void write(float percent);

  const uint8_t pin_;
  float duty_ = 0.0f;
  float target_ = 0.0f;
  uint32_t lastUpdateMs_ = 0;
};

}  // namespace gardener
