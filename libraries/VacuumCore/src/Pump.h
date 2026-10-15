#pragma once

#include <Arduino.h>

#include "VacuumConfig.h"

namespace eggplan {

// JQB2438 DC vacuum pump driven by a PWM signal into a low side MOSFET.
//
// The driver owns three pieces of policy: duty limiting, the slew rate limit
// (a diaphragm pump dislikes a step from 0 to full) and the maximum continuous
// run time that protects the pump when a control loop misbehaves.
class Pump {
 public:
  void begin();
  void update(uint32_t nowMs);

  // Requested duty in percent; approached gradually by update().
  void setDuty(float percent);
  // Bypass the ramp, e.g. for an immediate stop.
  void setDutyNow(float percent);
  void stop();
  // Cut the output immediately and latch a fault until resetFault().
  void emergencyStop(const char* reason);

  float dutyPercent() const { return duty_; }
  float targetPercent() const { return target_; }
  bool running() const { return duty_ >= 0.5f; }
  bool tripped() const { return tripped_; }
  const char* faultReason() const { return faultReason_; }
  bool resetFault();

  uint32_t runTimeMs() const;
  float rpm() const { return rpm_; }
  bool tachEnabled() const { return cfg::PUMP_TACH_ENABLED; }

  static void tachIsr(void* arg);

 private:
  void write(float percent);

  float duty_ = 0.0f;
  float target_ = 0.0f;
  uint32_t lastUpdateMs_ = 0;
  uint32_t runStartMs_ = 0;
  bool running_ = false;
  bool tripped_ = false;
  const char* faultReason_ = "";
  volatile uint32_t tachPulses_ = 0;
  uint32_t tachWindowStartMs_ = 0;
  uint32_t tachWindowPulses_ = 0;
  float rpm_ = 0.0f;
};

}  // namespace eggplan
