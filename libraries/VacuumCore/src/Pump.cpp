#include "Pump.h"

#include "Log.h"
#include "Util.h"

namespace eggplan {
namespace {
constexpr uint32_t kMaxStepMs = 250;
}

void Pump::begin() {
  pinMode(cfg::PIN_PUMP_PWM, OUTPUT);
  digitalWrite(cfg::PIN_PUMP_PWM, LOW);

  if (!ledcAttach(cfg::PIN_PUMP_PWM, cfg::PUMP_PWM_FREQ_HZ, cfg::PUMP_PWM_RESOLUTION)) {
    LOGE("pump: ledcAttach failed on GPIO%d", cfg::PIN_PUMP_PWM);
  }
  write(0.0f);

  if (cfg::PUMP_TACH_ENABLED) {
    pinMode(cfg::PIN_PUMP_TACH, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(cfg::PIN_PUMP_TACH), &Pump::tachIsr, this,
                       FALLING);
  }
  LOGI("pump: PWM on GPIO%d at %uHz, cap %.0f%%", cfg::PIN_PUMP_PWM,
       (unsigned)cfg::PUMP_PWM_FREQ_HZ, cfg::PUMP_DUTY_MAX_PERCENT);
}

void Pump::tachIsr(void* arg) {
  Pump* self = static_cast<Pump*>(arg);
  self->tachPulses_++;
}

void Pump::write(float percent) {
  const uint32_t maxDuty = (1u << cfg::PUMP_PWM_RESOLUTION) - 1u;
  percent = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  uint32_t duty = (uint32_t)((percent / 100.0f) * (float)maxDuty + 0.5f);
  ledcWrite(cfg::PIN_PUMP_PWM, duty);
}

void Pump::setDuty(float percent) {
  if (tripped_) {
    target_ = 0.0f;
    return;
  }
  target_ = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
}

void Pump::setDutyNow(float percent) {
  target_ = clampf(percent, 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  duty_ = target_;
  write(duty_);
}

void Pump::stop() { setDuty(0.0f); }

void Pump::emergencyStop(const char* reason) {
  target_ = 0.0f;
  duty_ = 0.0f;
  write(0.0f);
  tripped_ = true;
  faultReason_ = reason ? reason : "unspecified";
  running_ = false;
  LOGW("pump: emergency stop (%s)", faultReason_);
}

bool Pump::resetFault() {
  tripped_ = false;
  faultReason_ = "";
  LOGI("pump: fault cleared");
  return true;
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
    float maxStep = cfg::PUMP_RAMP_PERCENT_PER_SEC * ((float)dt / 1000.0f);
    if (maxStep < 0.5f) maxStep = 0.5f;
    float delta = target_ - duty_;
    duty_ += clampf(delta, -maxStep, maxStep);
    if (fabsf(duty_ - target_) < 0.05f) duty_ = target_;
    write(duty_);
  }

  bool wasRunning = running_;
  running_ = duty_ >= 0.5f;
  if (running_ && !wasRunning) {
    runStartMs_ = nowMs;
  }

  if (running_ && (nowMs - runStartMs_) > cfg::PUMP_MAX_RUN_MS) {
    emergencyStop("max run time exceeded");
    return;
  }

  if (cfg::PUMP_TACH_ENABLED) {
    uint32_t windowMs = nowMs - tachWindowStartMs_;
    if (windowMs >= 500) {
      noInterrupts();
      uint32_t pulses = tachPulses_;
      interrupts();
      uint32_t delta = pulses - tachWindowPulses_;
      tachWindowPulses_ = pulses;
      float revs = (float)delta / (float)cfg::PUMP_TACH_PULSES_PER_REV;
      rpm_ = revs * (60000.0f / (float)windowMs);
      tachWindowStartMs_ = nowMs;
    }
  }
}

}  // namespace eggplan
