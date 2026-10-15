#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "core/Config.h"
#include "hal/Pins.h"

namespace eggplan {

// JQB2438 DC vacuum pump driven by a PWM signal into a low side MOSFET.
//
// The driver owns three pieces of policy: the duty cap, the slew rate limit (a diaphragm
// pump dislikes a step from 0 to full) and the maximum continuous run time that protects
// the pump when a control loop misbehaves. Wiring comes from hal/Pins.h, so a board
// variant edits the pin map and nothing else.
class Pump {
 public:
  // False when there is no PWM pin or the LEDC channel could not be attached.
  bool begin();
  void update(uint32_t nowMs);

  void setDuty(float percent);
  void stop() { setDuty(0.0f); }
  // Cut the output immediately and latch a fault. The code is a wire token: it is what a
  // controller sees in `fault=`, and App is what turns it into something a person reads.
  void emergencyStop(const char* code);
  void resetFault();

  bool tripped() const { return tripped_; }
  const char* faultCode() const { return faultCode_; }
  float dutyPercent() const { return duty_; }

 private:
  void write(float percent);

  float duty_ = 0.0f;
  float target_ = 0.0f;
  uint32_t lastUpdateMs_ = 0;
  uint32_t runStartMs_ = 0;
  bool running_ = false;
  bool tripped_ = false;
  const char* faultCode_ = "";
};

}  // namespace eggplan
