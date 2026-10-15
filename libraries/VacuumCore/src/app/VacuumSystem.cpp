#include "app/VacuumSystem.h"

#include <Arduino.h>
#include <math.h>

#include "core/Math.h"

namespace eggplan {
namespace {
constexpr uint32_t kDerivativeWindowMs = 1000;
}

void VacuumSystem::begin() {
  rest();
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
}

void VacuumSystem::rest() {
  pump_.stop();
  vent_.open();  // normally open, but assert the safe resting state when asked
  pidReset();
  output_ = 0.0f;
  activity_ = Activity::Resting;
}

void VacuumSystem::vent() {
  pump_.stop();
  vent_.open();
  pidReset();
  output_ = 0.0f;
  ventStartMs_ = millis();
  activity_ = Activity::Venting;
}

void VacuumSystem::start() {
  // Close before pumping, so the pump never pulls against an open port.
  vent_.close();
  pidReset();
  output_ = 0.0f;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
  activity_ = Activity::Regulating;
}

bool VacuumSystem::vented() const {
  bool settled = sensor_.present() && sensor_.readingValid() &&
                 sensor_.kpa() >= sensor_.ambientKpa() - cfg::VENT_MARGIN_KPA;
  return settled || (millis() - ventStartMs_) >= cfg::VENT_TIMEOUT_MS;
}

float VacuumSystem::minReachableKpa() const {
  float ambient = sensor_.ambientKpa();
  float floorKpa = ambient - cfg::PUMP_MAX_SUCTION_KPA;
  return floorKpa > cfg::CONTROL_MIN_TARGET_KPA ? floorKpa : cfg::CONTROL_MIN_TARGET_KPA;
}

void VacuumSystem::setTarget(float kpa) {
  if (kpa <= cfg::CONTROL_MIN_TARGET_KPA) return;
  targetKpa_ = kpa;
  targetActive_ = true;
  pidReset();
}

void VacuumSystem::clearTarget() {
  targetActive_ = false;
  pidReset();
  output_ = 0.0f;
}

bool VacuumSystem::atTarget() const {
  return sensor_.kpa() <= targetKpa_ + cfg::CONTROL_DEADBAND_KPA;
}

void VacuumSystem::resetFault() {
  pump_.resetFault();
  faulted_ = false;
}

void VacuumSystem::fail(const char* code) {
  pump_.emergencyStop(code);
  vent_.open();  // release the coil: the safe direction
  pidReset();
  output_ = 0.0f;
  faulted_ = true;
  activity_ = Activity::Resting;
}

void VacuumSystem::service(uint32_t nowMs) {
  // A latched pump fault is terminal until the controller resets it.
  if (pump_.tripped()) {
    if (!faulted_) fail(pump_.faultCode());
    return;
  }

  if (activity_ != Activity::Regulating) return;

  // Regulating on a reading nobody trusts is how the loop runs away.
  if (!sensor_.present() || !sensor_.readingValid()) {
    fail("sensor");
    return;
  }

  regulate(nowMs);
}

// error is measured - target, so positive means "still needs vacuum"; the derivative term
// uses the measurement slew rather than the error slope, which keeps a setpoint change
// from kicking the output.
float VacuumSystem::pidUpdate(float dt, float error, float measurementSlew) {
  if (dt <= 0.0f) return cfg::PID_KP * error + pidIntegral_;
  pidIntegral_ = clampf(pidIntegral_ + cfg::PID_KI * error * dt, -cfg::PID_INTEGRAL_LIMIT,
                        cfg::PID_INTEGRAL_LIMIT);
  return cfg::PID_KP * error + pidIntegral_ - cfg::PID_KD * measurementSlew;
}

void VacuumSystem::regulate(uint32_t nowMs) {
  if (nowMs - lastControlMs_ < cfg::CONTROL_PERIOD_MS) return;

  float dt = (float)(nowMs - lastControlMs_) / 1000.0f;
  lastControlMs_ = nowMs;
  if (dt <= 0.0f) return;

  float measured = sensor_.kpa();
  float error = measured - targetKpa_;  // positive means "still needs vacuum"

  if (error < -cfg::CONTROL_DEADBAND_KPA) {
    // Deeper than requested; the pump cannot vent, so coast and let it bleed up.
    pidReset();
    output_ = 0.0f;
    pump_.stop();
    lastMeasured_ = measured;
    lastDerivativeMs_ = nowMs;
    return;
  }

  float controlError = fabsf(error) <= cfg::CONTROL_DEADBAND_KPA ? 0.0f : error;

  float slew = 0.0f;
  uint32_t windowMs = nowMs - lastDerivativeMs_;
  if (windowMs > 0 && windowMs <= kDerivativeWindowMs) {
    slew = (measured - lastMeasured_) / ((float)windowMs / 1000.0f);
  }

  output_ = clampf(pidUpdate(dt, controlError, slew), 0.0f, cfg::PUMP_DUTY_MAX_PERCENT);
  pump_.setDuty(output_);

  lastMeasured_ = measured;
  lastDerivativeMs_ = nowMs;
}

}  // namespace eggplan
