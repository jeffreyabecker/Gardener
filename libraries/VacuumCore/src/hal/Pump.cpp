#include "hal/Pump.h"

#include <math.h>

#include "core/Math.h"

namespace eggplan {
namespace {
constexpr uint32_t kMaxStepMs = 250;
}

bool Pump::begin() {
  if (config_.pwmPin < 0) return false;
  pinMode(config_.pwmPin, OUTPUT);
  digitalWrite(config_.pwmPin, LOW);
  if (!ledcAttach(config_.pwmPin, config_.pwmFreqHz, config_.pwmResolution)) return false;
  write(0.0f);
  tach_.begin(config_.tachPin, config_.tachPulsesPerRev);
  return true;
}

void Pump::write(float percent) {
  if (config_.pwmPin < 0) return;
  const uint32_t maxDuty = (1u << config_.pwmResolution) - 1u;
  float limited = clampf(percent, 0.0f, config_.maxDutyPercent);
  uint32_t duty = (uint32_t)((limited / 100.0f) * (float)maxDuty + 0.5f);
  ledcWrite(config_.pwmPin, duty);
}

void Pump::setDuty(float percent) {
  if (tripped_) {
    target_ = 0.0f;
    return;
  }
  target_ = clampf(percent, 0.0f, config_.maxDutyPercent);
}

void Pump::emergencyStop(const char* code, const char* text) {
  target_ = 0.0f;
  duty_ = 0.0f;
  write(0.0f);
  tripped_ = true;
  faultCode_ = code ? code : "unknown";
  faultText_ = text ? text : "unspecified";
  running_ = false;
}

void Pump::resetFault() {
  tripped_ = false;
  faultCode_ = "";
  faultText_ = "";
}

uint32_t Pump::runTimeMs() const {
  if (!running_) return 0;
  return millis() - runStartMs_;
}

void Pump::update(uint32_t nowMs) {
  if (lastUpdateMs_ == 0) lastUpdateMs_ = nowMs;
  uint32_t dt = nowMs - lastUpdateMs_;
  if (dt > kMaxStepMs) dt = kMaxStepMs;
  lastUpdateMs_ = nowMs;

  if (tripped_) {
    if (duty_ != 0.0f) {
      duty_ = 0.0f;
      write(duty_);
    }
    running_ = false;
    return;
  }

  // Slew rate limited approach to the requested duty.
  if (duty_ != target_) {
    float maxStep = config_.rampPercentPerSec * ((float)dt / 1000.0f);
    if (maxStep < 0.5f) maxStep = 0.5f;
    duty_ += clampf(target_ - duty_, -maxStep, maxStep);
    if (fabsf(duty_ - target_) < 0.05f) duty_ = target_;
    write(duty_);
  }

  bool wasRunning = running_;
  running_ = duty_ >= 0.5f;
  if (running_ && !wasRunning) runStartMs_ = nowMs;

  if (running_ && (nowMs - runStartMs_) > config_.maxRunMs) {
    emergencyStop("max_run", "max run time exceeded");
    return;
  }

  tach_.update(nowMs);
}

}  // namespace eggplan
