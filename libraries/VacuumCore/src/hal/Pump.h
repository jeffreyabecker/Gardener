#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "core/Config.h"
#include "hal/Tachometer.h"

namespace eggplan {

struct PumpConfig {
  int pwmPin = -1;
  int tachPin = -1;  // negative disables the tach input
  uint8_t tachPulsesPerRev = 2;
  uint32_t pwmFreqHz = 20000;
  uint8_t pwmResolution = 10;
  float maxDutyPercent = cfg::PUMP_DUTY_MAX_PERCENT;
  float rampPercentPerSec = cfg::PUMP_RAMP_PERCENT_PER_SEC;
  uint32_t maxRunMs = cfg::PUMP_MAX_RUN_MS;
};

// JQB2438 DC vacuum pump driven by a PWM signal into a low side MOSFET.
//
// The driver owns three pieces of policy from core/Config.h: the duty cap, the slew
// rate limit (a diaphragm pump dislikes a step from 0 to full) and the maximum
// continuous run time that protects the pump when a control loop misbehaves.
class Pump {
 public:
  explicit Pump(const PumpConfig& config) : config_(config) {}

  // False when there is no usable PWM pin or the LEDC channel could not be attached.
  bool begin();
  void update(uint32_t nowMs);

  void setDuty(float percent);
  void stop() { setDuty(0.0f); }
  // Cut the output immediately and latch a fault. code is a wire token, text is for
  // humans and never leaves the log.
  void emergencyStop(const char* code, const char* text);
  void resetFault();

  bool tripped() const { return tripped_; }
  const char* faultCode() const { return faultCode_; }
  const char* faultText() const { return faultText_; }
  float dutyPercent() const { return duty_; }
  float targetPercent() const { return target_; }
  bool running() const { return duty_ >= 0.5f; }
  uint32_t runTimeMs() const;
  float rpm() const { return tach_.rpm(); }
  bool tachEnabled() const { return tach_.enabled(); }

 private:
  void write(float percent);

  PumpConfig config_;
  Tachometer tach_;
  float duty_ = 0.0f;
  float target_ = 0.0f;
  uint32_t lastUpdateMs_ = 0;
  uint32_t runStartMs_ = 0;
  bool running_ = false;
  bool tripped_ = false;
  const char* faultCode_ = "";
  const char* faultText_ = "";
};

}  // namespace eggplan
