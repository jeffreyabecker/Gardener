#include "app/VacuumSystem.h"

#include <Arduino.h>
#include <math.h>

#include "core/Math.h"

namespace gardener {
namespace {
constexpr uint32_t kDerivativeWindowMs = 1000;
}

// A latched fault owns the state until it is reset: a command can still make the plant safe,
// but none of them can move the state off Fault, so state() == Fault and faulted() agree.
void VacuumSystem::setState(RunState state) {
  if (faulted_ && state != RunState::Fault) return;
  state_ = state;
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
  setState(RunState::Idle);
}

void VacuumSystem::vent() {
  pump_.stop();
  vent_.open();
  pidReset();
  output_ = 0.0f;
  ventStartMs_ = millis();
  setState(RunState::Venting);
}

bool VacuumSystem::start() {
  if (faulted_) return false;
  if (!targetActive_) return false;
  if (!sensor_.present() || !sensor_.readingValid()) return false;

  // Close before pumping, so the pump never pulls against an open port.
  vent_.close();
  pidReset();
  output_ = 0.0f;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
  setState(RunState::Pulling);
  return true;
}

void VacuumSystem::stop() {
  // An idle plant is already released, so there is nothing to vent.
  if (state_ == RunState::Idle) {
    rest();
    return;
  }
  vent();
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

  // A run without a target is not a run, so clearing one releases the chamber. An idle plant
  // is already at rest, and a fault owns the state until it is reset.
  if (state_ == RunState::Pulling || state_ == RunState::Holding) {
    vent();
  } else if (state_ == RunState::Idle) {
    rest();
  }
}

bool VacuumSystem::atTarget() const {
  return sensor_.kpa() <= targetKpa_ + cfg::CONTROL_DEADBAND_KPA;
}

void VacuumSystem::resetFault() {
  pump_.resetFault();
  faulted_ = false;
  // The latch is what held the state, so clearing it has to put the machine somewhere: a
  // fault is cleared by releasing the chamber, which is where one leaves it anyway.
  if (state_ == RunState::Fault) vent();
}

void VacuumSystem::fail(const char* code) {
  pump_.emergencyStop(code);
  vent_.open();  // release the coil: the safe direction
  pidReset();
  output_ = 0.0f;
  faulted_ = true;
  setState(RunState::Fault);
}

void VacuumSystem::service(uint32_t nowMs) {
  // The pump latches its own output off after an emergency stop, and a latch the system did
  // not ask for still has to become a fault. Today fail() is the only thing that trips it.
  if (pump_.tripped()) {
    if (!faulted_) fail(pump_.faultCode());
    return;
  }

  const bool regulating = state_ == RunState::Pulling || state_ == RunState::Holding;

  // Regulating on a reading nobody trusts is how the loop runs away.
  if (regulating && (!sensor_.present() || !sensor_.readingValid())) {
    fail("sensor");
    return;
  }

  // The two transitions that are facts about the chamber rather than orders: the target has
  // been reached, and the release is over. A latched fault owns the state, so neither can
  // fire while one is held.
  if (state_ == RunState::Pulling && atTarget()) setState(RunState::Holding);
  if (state_ == RunState::Venting && vented()) {
    rest();
    return;
  }

  if (regulating) regulate(nowMs);
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

}  // namespace gardener
