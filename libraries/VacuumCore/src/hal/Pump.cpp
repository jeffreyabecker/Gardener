#include "hal/Pump.h"

#include <math.h>

#include "core/Math.h"

namespace gardener {
namespace {
constexpr uint32_t kMaxStepMs = 250;
}

bool Pump::begin() {
  pinMode(pins::PUMP_PWM, OUTPUT);
  digitalWrite(pins::PUMP_PWM, LOW);
  if (!ledcAttach(pins::PUMP_PWM, pins::PUMP_PWM_FREQ_HZ, pins::PUMP_PWM_RESOLUTION)) {
    return false;
  }
  write(0.0f);
  return true;
}

void Pump::write(float percent) {
  const uint32_t maxDuty = (1u << pins::PUMP_PWM_RESOLUTION) - 1u;
  float limited = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  uint32_t duty = (uint32_t)((limited / 100.0f) * (float)maxDuty + 0.5f);
  ledcWrite(pins::PUMP_PWM, duty);
}

void Pump::setDuty(float percent) {
  if (tripped_) {
    target_ = 0.0f;
    return;
  }
  target_ = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
}

void Pump::emergencyStop(const char* code) {
  target_ = 0.0f;
  duty_ = 0.0f;
  write(0.0f);
  tripped_ = true;
  faultCode_ = code ? code : "unknown";
}

void Pump::resetFault() {
  tripped_ = false;
  faultCode_ = "";
}

void Pump::update(uint32_t nowMs) {
  if (lastUpdateMs_ == 0) lastUpdateMs_ = nowMs;
  uint32_t dt = nowMs - lastUpdateMs_;
  if (dt > kMaxStepMs) dt = kMaxStepMs;
  lastUpdateMs_ = nowMs;

  // A latched emergency stop holds the output at zero until it is reset.
  if (tripped_) {
    if (duty_ != 0.0f) {
      duty_ = 0.0f;
      write(duty_);
    }
    return;
  }

  // Slew rate limited approach to the requested duty.
  if (duty_ != target_) {
    float maxStep = cfg::PUMP_RAMP_PERCENT_PER_SEC * ((float)dt / 1000.0f);
    if (maxStep < 0.5f) maxStep = 0.5f;
    duty_ += clampf(target_ - duty_, -maxStep, maxStep);
    if (fabsf(duty_ - target_) < 0.05f) duty_ = target_;
    write(duty_);
  }
}

}  // namespace gardener
