#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "core/Config.h"
#include "hal/Pins.h"

namespace gardener {

// JQB2438 DC vacuum pump driven by a PWM signal into a low side MOSFET.
//
// The driver owns two pieces of policy: the duty cap, and the slew rate limit (a diaphragm pump
// dislikes a step from 0 to full). Wiring comes from hal/Pins.h, so a board variant edits the pin
// map and nothing else.
//
// It carries no fault latch of its own: the plant holds that one latch and the fault code with it.
// All a fault asks of the driver is halt() — the output to zero now, rather than through the ramp.
class Pump {
 public:
  // False when there is no PWM pin or the LEDC channel could not be attached.
  bool begin();
  void update(uint32_t nowMs);

  void setDuty(float percent);
  void stop() { setDuty(0.0f); }
  // Cut the output immediately. The ramp is for duty changes the loop asked for, not for a fault.
  void halt();

  float dutyPercent() const { return duty_; }

 private:
  void write(float percent);

  float duty_ = 0.0f;
  float target_ = 0.0f;
  uint32_t lastUpdateMs_ = 0;
};

}  // namespace gardener
