#include "hal/impl/PwmPump.h"

#include <Arduino.h>
#include <math.h>
#include <type_traits>

#include "core/Config.h"
#include "core/Math.h"
#include "hal/Pins.h"

namespace gardener {
namespace {
constexpr uint32_t kMaxStepMs = 250;

// The LEDC channel's own setup, which is a property of the drive rather than of the loop: a
// switching frequency above hearing, and ten bits of resolution for the duty.
constexpr uint32_t kPwmFreqHz = 20000;
constexpr uint8_t kPwmResolution = 10;
}  // namespace

// Nothing else spells the interface out, so a verb added to Pump and not implemented here would
// leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<PwmPump>::value, "PwmPump must implement every Pump verb");

bool PwmPump::begin() {
  pinMode(pins::PUMP_PWM, OUTPUT);
  digitalWrite(pins::PUMP_PWM, LOW);
  if (!ledcAttach(pins::PUMP_PWM, kPwmFreqHz, kPwmResolution)) {
    return false;
  }
  write(0.0f);
  return true;
}

void PwmPump::write(float percent) {
  const uint32_t maxDuty = (1u << kPwmResolution) - 1u;
  float limited = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  uint32_t duty = (uint32_t)((limited / 100.0f) * (float)maxDuty + 0.5f);
  ledcWrite(pins::PUMP_PWM, duty);
}

void PwmPump::setDuty(float percent) {
  target_ = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
}

void PwmPump::halt() {
  target_ = 0.0f;
  duty_ = 0.0f;
  write(0.0f);
}

void PwmPump::update(uint32_t nowMs) {
  if (lastUpdateMs_ == 0) lastUpdateMs_ = nowMs;
  uint32_t dt = nowMs - lastUpdateMs_;
  if (dt > kMaxStepMs) dt = kMaxStepMs;
  lastUpdateMs_ = nowMs;

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
