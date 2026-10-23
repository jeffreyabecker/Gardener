#include "VacuumSystem.h"

#include <Arduino.h>
#include <math.h>

#include "Machine.h"
#include "core/Math.h"
#include "storage/MachineSettings.h"

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

float VacuumSystem::currentPressureKpa() const {
  return sensor_->present() && sensor_->readingValid() ? sensor_->pressureKpa() : 0.0f;
}

void VacuumSystem::rest() {
  pump_->stop();
  vent_->open();  // assert the vented resting state when asked
  pidReset();
  output_ = 0.0f;
  targetVent_ = false;
  setState(RunState::Idle);
}

void VacuumSystem::release() {
  pump_->stop();
  vent_->open();
  pidReset();
  output_ = 0.0f;
  targetVent_ = false;
  ventStartMs_ = millis();
  setState(RunState::Venting);
}

bool VacuumSystem::start() {
  if (startRefusal() != nullptr) return false;

  // Close before pumping, so the pump never pulls against an open port.
  vent_->close();
  targetVent_ = false;
  pidReset();
  output_ = 0.0f;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
  setState(RunState::Pulling);
  return true;
}

const char* VacuumSystem::startRefusal() const {
  if (faulted()) return "latch";
  if (targetPressureKpa_ <= 0.0f) return "no_target";
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

bool VacuumSystem::released() const {
  bool settled = sensor_->present() && sensor_->readingValid() &&
                 currentPressureKpa() <= VENT_MARGIN_KPA;
  return settled || (millis() - ventStartMs_) >= VENT_TIMEOUT_MS;
}

// Stop, then open: the pump never pulls against an open port. The state is VENT either way, and
// targetVent_ is what tells the service loop which ending to wait for.
void VacuumSystem::startTargetVent() {
  pump_->stop();
  vent_->open();
  pidReset();
  output_ = 0.0f;
  targetVent_ = true;
  ventStartMs_ = millis();
  setState(RunState::Venting);
}

// The landing is done: seal the chamber and hand it back to the run's state machine. Resetting the
// derivative state keeps the valve's slew out of the PID's first step.
void VacuumSystem::endTargetVent() {
  vent_->close();
  targetVent_ = false;
  pidReset();
  output_ = 0.0f;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
  lastMeasured_ = currentPressureKpa();
  setState(RunState::Pulling);
}

void VacuumSystem::setTarget(float pressureKpa) {
  // Shallower than the loop's floor is not a target, and the floor itself is one: the bound the
  // wire publishes is this constant, so a target of exactly it has to be taken. Deeper than the
  // pump can reach still is a target: the duty pins at the cap and the run simply never gets there.
  if (pressureKpa < CONTROL_MIN_TARGET_KPA) return;
  targetPressureKpa_ = pressureKpa;
  pidReset();

  // A target shallower than the chamber cannot be pulled to, so a run in progress uses the valve.
  // A change inside the deadband is already at target and needs neither actuator. A vent already
  // running follows the new target, or ends when the target moves back to where the pump works.
  const float deadband = settings_.deadbandPressureKpa();
  const float measured = currentPressureKpa();
  const bool active = state_ == RunState::Pulling || state_ == RunState::Holding;
  const bool shallower = measured > pressureKpa + deadband;
  if (targetVent_) {
    if (!shallower) endTargetVent();
    return;
  }
  if (!active) return;
  if (shallower) {
    startTargetVent();
  } else if (pressureKpa > measured + deadband) {
    // A deeper target is a pull from here. HOLD is the target-reached edge, so keeping it while
    // the machine works to a new depth would hide the edge the controller times from.
    setState(RunState::Pulling);
  }
}

void VacuumSystem::clearTarget() {
  targetPressureKpa_ = 0.0f;
  pidReset();
  output_ = 0.0f;

  // A run without a target is not a run, so clearing one releases the chamber. A target vent is
  // cut into that release rather than landing on zero. An idle vacuum system is already at rest,
  // and a fault owns the state until it is reset.
  if (targetVent_ || state_ == RunState::Pulling || state_ == RunState::Holding) {
    release();
  } else if (state_ == RunState::Idle) {
    rest();
  }
}

bool VacuumSystem::atTarget() const {
  return currentPressureKpa() >= targetPressureKpa_ - settings_.deadbandPressureKpa();
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
  targetVent_ = false;
  faultCode_ = code ? code : "unknown";
  setState(RunState::Fault);
}

void VacuumSystem::service(uint32_t nowMs) {
  // A latched fault owns the state until it is reset: the vacuum system is already safe, and the
  // code is what `vacuum get` reports.
  if (faulted()) return;

  const bool running = state_ == RunState::Pulling || state_ == RunState::Holding;

  // Regulating on a reading nobody trusts is how the loop runs away. A target vent steers on the
  // reading too; a release can finish on its timeout, so a lost sensor does not have to fault it.
  if ((running || targetVent_) && (!sensor_->present() || !sensor_->readingValid())) {
    fail("sensor");
    return;
  }

  // VENT is one state with two endings. A target vent seals the chamber and hands it back to the
  // run; a release waits for ambient or the timeout and goes to rest.
  if (state_ == RunState::Venting) {
    if (targetVent_) {
      const float landing = CONTROL_VENT_TO_TARGET
                                ? targetPressureKpa_ + VENT_TARGET_LEAD_KPA
                                : targetPressureKpa_;
      const bool arrived = currentPressureKpa() <= landing;
      const bool timedOut = (nowMs - ventStartMs_) >= VENT_TIMEOUT_MS;
      if (!arrived && !timedOut) return;
      endTargetVent();
    } else {
      if (released()) rest();
      return;
    }
  }

  // The two transitions that are facts about the chamber rather than orders: the target has
  // been reached, and the release is over. A latched fault owns the state, so neither can
  // fire while one is held.
  if (state_ == RunState::Pulling && atTarget()) setState(RunState::Holding);
  if (state_ == RunState::Pulling || state_ == RunState::Holding) regulate(nowMs);
}

// error is how far the chamber still is from the target, so positive means "still needs
// vacuum"; the derivative term uses the measurement slew rather than the error slope, which
// keeps a setpoint change from kicking the output. The gains are derived from the settings the
// loop was handed, so a pump or chamber change retunes it without touching a gain.
float VacuumSystem::pidUpdate(float dt, float error, float measurementSlew) {
  const PidGains gains = pidGainsFor(settings_.pumpFlowLpm(), settings_.chamberVolumeL());
  if (dt <= 0.0f) return gains.kp * error + pidIntegral_;
  pidIntegral_ = clampf(pidIntegral_ + gains.ki * error * dt, -gains.integralLimit,
                        gains.integralLimit);
  return gains.kp * error + pidIntegral_ - gains.kd * measurementSlew;
}

void VacuumSystem::regulate(uint32_t nowMs) {
  if (nowMs - lastControlMs_ < CONTROL_PERIOD_MS) return;

  // A retune — a loop input the settings hold changed — starts the integral over, as a target
  // change already does: the accumulated error is in the old gains' units.
  if (settings_.revision() != lastGainsRevision_) {
    pidReset();
    lastGainsRevision_ = settings_.revision();
  }

  float dt = (float)(nowMs - lastControlMs_) / 1000.0f;
  lastControlMs_ = nowMs;
  if (dt <= 0.0f) return;

  const float deadband = settings_.deadbandPressureKpa();
  const float maxDuty = settings_.pumpMaxPercent();

  // The slew comes off the same reading the error is built from, so the derivative gain is
  // signed against it.
  float measured = currentPressureKpa();
  float error = targetPressureKpa_ - measured;  // positive means "still needs vacuum"

  if (error < -deadband) {
    // Deeper than requested; the pump cannot vent, so coast and let it leak back toward ambient.
    pidReset();
    output_ = 0.0f;
    pump_->stop();
    lastMeasured_ = measured;
    lastDerivativeMs_ = nowMs;
    return;
  }

  float controlError = fabsf(error) <= deadband ? 0.0f : error;

  float slew = 0.0f;
  uint32_t windowMs = nowMs - lastDerivativeMs_;
  if (windowMs > 0 && windowMs <= kDerivativeWindowMs) {
    slew = (measured - lastMeasured_) / ((float)windowMs / 1000.0f);
  }

  output_ = clampf(pidUpdate(dt, controlError, slew), 0.0f, maxDuty);
  pump_->setDuty(output_);

  lastMeasured_ = measured;
  lastDerivativeMs_ = nowMs;
}

}  // namespace gardener
