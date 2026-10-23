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
  // A chamber that comes up above the vent floor is not at rest: report the release, and let the
  // floor or the timeout finish it.
  if (sensor_->present() && sensor_->readingValid() && !released()) release();
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
  overshootReadings_ = 0;
  setState(RunState::Idle);
}

void VacuumSystem::release() {
  pump_->stop();
  vent_->open();
  pidReset();
  output_ = 0.0f;
  targetVent_ = false;
  overshootReadings_ = 0;
  ventStartMs_ = millis();
  setState(RunState::Venting);
}

bool VacuumSystem::start(uint32_t holdSeconds) {
  if (startRefusal() != nullptr) return false;
  // A start while already running is a no-op: the run that is under way owns its deadline.
  if (running()) return true;

  // Close before pumping, so the pump never pulls against an open port.
  vent_->close();
  targetVent_ = false;
  pidReset();
  output_ = 0.0f;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;

  runStartMs_ = lastControlMs_;
  const uint32_t maxHoldSeconds = CONTROL_MAX_RUN_MS / 1000;
  if (holdSeconds > maxHoldSeconds) holdSeconds = maxHoldSeconds;
  holdMs_ = holdSeconds == 0 ? CONTROL_MAX_RUN_MS : holdSeconds * 1000;
  deadlineExpired_ = false;
  progressMs_ = runStartMs_;
  bestDepthKpa_ = currentPressureKpa();
  overshootReadings_ = 0;

  // The chamber may already be shallower or deeper than the target; the one decision says which
  // actuator gets it there.
  converge();
  return true;
}

const char* VacuumSystem::startRefusal() const {
  if (faulted()) return "latch";
  if (!sensor_->present() || !sensor_->readingValid()) return "no_reading";
  return nullptr;
}

void VacuumSystem::stop() {
  // An idle vacuum system is already released, so there is nothing to vent.
  if (state_ == RunState::Idle) {
    rest();
    return;
  }
  // A release already under way is the same stop: restarting its timer would stretch the vent.
  if (state_ == RunState::Venting && !targetVent_) return;
  release();
}

bool VacuumSystem::running() const {
  return state_ == RunState::Pulling || state_ == RunState::Holding || targetVent_;
}

bool VacuumSystem::takeDeadlineExpired() {
  const bool expired = deadlineExpired_;
  deadlineExpired_ = false;
  return expired;
}

bool VacuumSystem::released() const {
  return sensor_->present() && sensor_->readingValid() &&
         currentPressureKpa() <= VENT_FLOOR_KPA + VENT_MARGIN_KPA;
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

// Seal the chamber and hand it back to the run. A vent that has just landed, and a target that is
// already where the pump works, both arrive here, so the derivative's first step never sees the
// valve's slew.
void VacuumSystem::closeValve() {
  vent_->close();
  targetVent_ = false;
  pidReset();
  output_ = 0.0f;
  lastControlMs_ = millis();
  lastDerivativeMs_ = lastControlMs_;
  lastMeasured_ = currentPressureKpa();
  bestDepthKpa_ = lastMeasured_;
  progressMs_ = lastControlMs_;
}

void VacuumSystem::endTargetVent() {
  closeValve();
  setState(RunState::Pulling);
}

// The one decision, from the command's outcome, the state and the reading: a target of 0 releases,
// a reading shallower than the target opens the valve, and anything else closes it and lets the
// loop work. Every command ends here, so the vent/start/stop choice is made once.
void VacuumSystem::converge() {
  const float deadband = settings_.deadbandPressureKpa();
  const float measured = currentPressureKpa();

  // Ambient is the release, whichever way it was asked for.
  if (targetPressureKpa_ <= 0.0f) {
    if (state_ != RunState::Idle) release();
    return;
  }

  // Shallower than the chamber: the pump cannot vent, so the valve does, and the landing hands the
  // run back to the loop.
  if (measured > targetPressureKpa_ + deadband) {
    if (!targetVent_) startTargetVent();
    return;
  }

  closeValve();
  setState(measured >= targetPressureKpa_ - deadband ? RunState::Holding : RunState::Pulling);
}

void VacuumSystem::setTarget(float pressureKpa) {
  // The bounds are the machine's, not the wire's: 0 is ambient, and anything deeper than the
  // fitted floor is a depth the pump cannot reach. A refused value leaves the target unchanged.
  if (!(pressureKpa >= 0.0f) || pressureKpa > settings_.floorPressureKpa()) return;
  if (pressureKpa == targetPressureKpa_) return;
  targetPressureKpa_ = pressureKpa;
  pidReset();

  // A target written while nothing is running is stored, not acted on.
  if (!running()) return;
  converge();
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

// A run has a deadline. Expiry is a release, not a fault: the machine did what it was told for as
// long as it was allowed to, and saying so is the run log's business.
void VacuumSystem::checkRun(uint32_t nowMs) {
  if (state_ != RunState::Pulling && state_ != RunState::Holding) return;
  if (holdMs_ == 0 || nowMs - runStartMs_ < holdMs_) return;
  deadlineExpired_ = true;
  release();
}

// Five readings more than the overshoot margin past the target is the loop not holding what it was
// asked to, so the chamber is released rather than pulled further out of range.
void VacuumSystem::checkOvershoot(float measured) {
  if (measured <= targetPressureKpa_ + CONTROL_OVERSHOOT_KPA) {
    overshootReadings_ = 0;
    return;
  }
  if (++overshootReadings_ >= CONTROL_OVERSHOOT_READINGS) fail("overshoot");
}

void VacuumSystem::service(uint32_t nowMs) {
  // A latched fault owns the state until it is reset: the vacuum system is already safe, and the
  // code is what `vacuum get` reports.
  if (faulted()) return;

  // Regulating on a reading nobody trusts is how the loop runs away, and a release cannot see the
  // floor without one. Only the resting state is allowed to be blind: it is vented already.
  if (state_ != RunState::Idle && (!sensor_->present() || !sensor_->readingValid())) {
    fail("sensor");
    return;
  }

  checkRun(nowMs);

  // VENT is one state with two endings. A target vent seals the chamber and hands it back to the
  // run; a release waits for this rig's vent floor and is a fault if it never gets there.
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
      if (released()) {
        rest();
        return;
      }
      if ((nowMs - ventStartMs_) >= VENT_TIMEOUT_MS) fail("vent_timeout");
      return;
    }
  }

  // The two transitions that are facts about the chamber rather than orders: the target has
  // been reached, and the release is over. A latched fault owns the state, so neither can
  // fire while one is held.
  if (state_ == RunState::Pulling && atTarget()) setState(RunState::Holding);
  if (state_ == RunState::Pulling || state_ == RunState::Holding) {
    checkOvershoot(currentPressureKpa());
    if (faulted()) return;
    regulate(nowMs);
  }
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

// The headway a pull makes: a reading a kilopascal deeper than the best so far restarts the window.
// Driven at or above half the duty cap for a whole window without that much progress is the pump
// working against something it cannot move, and no controller has to notice it.
void VacuumSystem::checkProgress(uint32_t nowMs, float measured, float maxDuty) {
  if (measured >= bestDepthKpa_ + CONTROL_PROGRESS_KPA) {
    bestDepthKpa_ = measured;
    progressMs_ = nowMs;
    return;
  }
  if (output_ < maxDuty * CONTROL_PROGRESS_DUTY_FRACTION) {
    progressMs_ = nowMs;
    return;
  }
  if (nowMs - progressMs_ >= CONTROL_PROGRESS_TIMEOUT_MS) fail("no_progress");
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

  if (state_ == RunState::Pulling) checkProgress(nowMs, measured, maxDuty);

  lastMeasured_ = measured;
  lastDerivativeMs_ = nowMs;
}

}  // namespace gardener
