#include "hal/impl/PwmPump.h"

#include <Arduino.h>
#include <math.h>
#include <type_traits>

#include "core/Knob.h"
#include "core/Math.h"

namespace gardener::runtime {
namespace {
GARDENER_KNOB_VALID(uint32_t, MaxStepMs, GARDENER_PUMP_MAX_STEP_MS, 250,
                    GARDENER_PUMP_MAX_STEP_MS > 0,
                    "the longest the ramp waits between duty steps, in milliseconds");

// The LEDC channel's own setup, which is a property of the drive rather than of the loop: a
// switching frequency above hearing, and ten bits of resolution for the duty.
GARDENER_KNOB_VALID(uint32_t, PwmFreqHz, GARDENER_PUMP_PWM_FREQ_HZ, 20000,
                    GARDENER_PUMP_PWM_FREQ_HZ >= 1000 && GARDENER_PUMP_PWM_FREQ_HZ <= 40000,
                    "the switching frequency, above hearing, in hertz");
GARDENER_KNOB_VALID(uint8_t, PwmResolution, GARDENER_PUMP_PWM_RESOLUTION, 10,
                    GARDENER_PUMP_PWM_RESOLUTION >= 1 && GARDENER_PUMP_PWM_RESOLUTION <= 16,
                    "the duty resolution, in bits");
}  // namespace

// Nothing else spells the interface out, so a verb added to Pump and not implemented here would
// leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<PwmPump>::value, "PwmPump must implement every Pump verb");

bool PwmPump::begin() {
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
  if (!ledcAttach(pin_, PwmFreqHz, PwmResolution)) {
    return false;
  }
  write(0.0f);
  return true;
}

void PwmPump::write(float percent) {
  const uint32_t maxDuty = (1u << PwmResolution) - 1u;
  float limited = clampf(percent, 0.0f, dutyMax_);
  uint32_t duty = (uint32_t)((limited / 100.0f) * (float)maxDuty + 0.5f);
  ledcWrite(pin_, duty);
}

void PwmPump::setDuty(float percent) { target_ = clampf(percent, 0.0f, dutyMax_); }

void PwmPump::halt() {
  target_ = 0.0f;
  duty_ = 0.0f;
  write(0.0f);
}

void PwmPump::update(uint32_t nowMs) {
  if (lastUpdateMs_ == 0) lastUpdateMs_ = nowMs;
  uint32_t dt = nowMs - lastUpdateMs_;
  if (dt > MaxStepMs) dt = MaxStepMs;
  lastUpdateMs_ = nowMs;

  // Slew rate limited approach to the requested duty.
  if (duty_ != target_) {
    float maxStep = rampPerSec_ * ((float)dt / 1000.0f);
    if (maxStep < 0.5f) maxStep = 0.5f;
    duty_ += clampf(target_ - duty_, -maxStep, maxStep);
    if (fabsf(duty_ - target_) < 0.05f) duty_ = target_;
    write(duty_);
  }
}

}  // namespace gardener::runtime