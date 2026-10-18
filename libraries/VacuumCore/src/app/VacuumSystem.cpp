#include "app/VacuumSystem.h"

#include <Arduino.h>
#include <math.h>

#include "core/Math.h"

namespace gardener {
namespace {
constexpr uint32_t kDerivativeWindowMs = 1000;
}  // namespace

// A latched fault owns the state until it is reset: a command can still make the vacuum system safe,
// but none of them can move the state off Fault, so runState() == Fault and faulted() agree.
void VacuumSystem::setState(RunState state) {
  if (faulted() && state != RunState::Fault) return;
  state_ = state;
}

void VacuumSystem::begin() {
  rest();
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
}

float VacuumSystem::currentKpa() const {
  return sensor_->readingValid() ? sensor_->kpa() : 0.0f;
}

void VacuumSystem::rest() {
  pump_->stop();
  vent_->open();  // assert the vented resting state when asked
  pidReset();
  output_ = 0.0f;
  setState(RunState::Idle);
}

void VacuumSystem::release() {
  pump_->stop();
  vent_->open();
  pidReset();
  output_ = 0.0f;
  ventStartMs_ = millis();
  setState(RunState::Venting);
}

bool VacuumSystem::start() {
  if (startRefusal() != nullptr) return false;

  // Close before pumping, so the pump never pulls against an open port.
  vent_->close();
  pidReset();
  output_ = 0.0f;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
  setState(RunState::Pulling);
  return true;
}

const char* VacuumSystem::startRefusal() const {
  if (faulted()) return "latch";
  if (targetKpa_ <= 0.0f) return "no_target";
  if (!sensor_->present() || !sensor_->readingValid()) return "no_reading";
  return nullptr;
}

void VacuumSystem::stop() {
  // An idle vacuum system is already released, so there is nothing to vent.
  if (state_ == RunState::Idle) {
    rest();
    return;
  }
  release();
}

bool VacuumSystem::vented() const {
  bool settled = sensor_->present() && sensor_->readingValid() &&
                 currentKpa() <= cfg::VENT_MARGIN_KPA;
  return settled || (millis() - ventStartMs_) >= cfg::VENT_TIMEOUT_MS;
}

void VacuumSystem::setTarget(float kpa) {
  // Shallower than the loop's floor is not a target, and the floor itself is one: the bound the
  // wire publishes is this constant, so a target of exactly it has to be taken. Deeper than the
  // pump can reach still is a target: the duty pins at the cap and the run simply never gets there.
  if (kpa < cfg::CONTROL_MIN_TARGET_KPA) return;
  targetKpa_ = kpa;
  pidReset();
}

void VacuumSystem::clearTarget() {
  targetKpa_ = 0.0f;
  pidReset();
  output_ = 0.0f;

  // A run without a target is not a run, so clearing one releases the chamber. An idle vacuum
  // system is already at rest, and a fault owns the state until it is reset.
  if (state_ == RunState::Pulling || state_ == RunState::Holding) {
    release();
  } else if (state_ == RunState::Idle) {
    rest();
  }
}

bool VacuumSystem::atTarget() const {
  return currentKpa() >= targetKpa_ - cfg::CONTROL_DEADBAND_KPA;
}

void VacuumSystem::resetFault() {
  faultCode_ = "";
  // The latch is what held the state, so clearing it has to put the machine somewhere: a
  // fault is cleared by releasing the chamber, which is where one leaves it anyway.
  if (state_ == RunState::Fault) release();
}

// The latch and the safe state, in that order: the output is cut now rather than ramped, the valve
// is opened, and the code is what `vacuum get` reports until resetFault(). Nothing writes a duty
// again while it is set, because nothing but resetFault() leaves Fault.
void VacuumSystem::fail(const char* code) {
  pump_->halt();
  vent_->open();  // drive the valve open: the safe direction
  pidReset();
  output_ = 0.0f;
  faultCode_ = code ? code : "unknown";
  setState(RunState::Fault);
}

void VacuumSystem::service(uint32_t nowMs) {
  // A latched fault owns the state until it is reset: the vacuum system is already safe, and the
  // code is what `vacuum get` reports.
  if (faulted()) return;

  const bool regulating = state_ == RunState::Pulling || state_ == RunState::Holding;

  // Regulating on a reading nobody trusts is how the loop runs away.
  if (regulating && (!sensor_->present() || !sensor_->readingValid())) {
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

// error is how far the chamber still is from the target, so positive means "still needs
// vacuum"; the derivative term uses the measurement slew rather than the error slope, which
// keeps a setpoint change from kicking the output.
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

  // The slew comes off the same reading the error is built from, so PID_KD is signed against it.
  float measured = currentKpa();
  float error = targetKpa_ - measured;  // positive means "still needs vacuum"

  if (error < -cfg::CONTROL_DEADBAND_KPA) {
    // Deeper than requested; the pump cannot vent, so coast and let it bleed up.
    pidReset();
    output_ = 0.0f;
    pump_->stop();
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
  pump_->setDuty(output_);

  lastMeasured_ = measured;
  lastDerivativeMs_ = nowMs;
}

}  // namespace gardener
