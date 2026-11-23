#include "machine/VacuumSystem.h"

#include <Arduino.h>

#include "machine/Machine.h"

namespace gardener {
namespace {

// The band either side of a target, as depths: under the first the pump runs, over the second the
// valve opens. Both are a percentage of the target, so the band travels with it.
float shallowEdgeKpa(float targetKpa) { return targetKpa * (1.0f - ControlBelowPercent / 100.0f); }
float deepEdgeKpa(float targetKpa) { return targetKpa * (1.0f + ControlAbovePercent / 100.0f); }

}  // namespace

// A latched fault owns the state until it is reset: a command can still make the vacuum system safe,
// but none of them can move the state off Fault, so runState() == Fault and faulted() agree.
void VacuumSystem::setState(RunState state) {
  if (faulted() && state != RunState::Fault) return;
  if (state == RunState::Holding && state_ != RunState::Holding) holdStartMs_ = millis();
  state_ = state;
}

void VacuumSystem::begin() {
  rest();
  // A chamber that comes up above the vent floor is not at rest: report the release, and let the
  // floor or the timeout finish it.
  if (sensor_->present() && sensor_->readingValid() && !released()) release();
}

float VacuumSystem::currentPressureKpa() const {
  return sensor_->present() && sensor_->readingValid() ? sensor_->pressureKpa() : 0.0f;
}

void VacuumSystem::rest() {
  pump_->off();
  vent_->open();  // assert the vented resting state when asked
  ventingToTarget_ = false;
  overshootReadings_ = 0;
  overshootRecoveries_ = 0;
  setState(RunState::Idle);
}

void VacuumSystem::release() {
  rest();                   // pump off, valve open, the loop forgotten
  ventStartMs_ = millis();  // this vent is timed
  setState(RunState::Venting);
}

bool VacuumSystem::start(uint32_t holdSeconds) {
  if (startRefusal() != nullptr) return false;
  // A start while already running is a no-op: the run that is under way owns its deadline.
  if (running()) return true;

  // Close before pumping, so the pump never pulls against an open port.
  vent_->close();
  ventingToTarget_ = false;

  runStartMs_ = millis();
  const uint32_t maxHoldSeconds = ControlMaxRunMs / 1000;
  if (holdSeconds > maxHoldSeconds) holdSeconds = maxHoldSeconds;
  holdMs_ = holdSeconds == 0 ? ControlMaxRunMs : holdSeconds * 1000;
  deadlineExpired_ = false;
  progressMs_ = runStartMs_;
  bestDepthKpa_ = currentPressureKpa();
  overshootReadings_ = 0;
  overshootRecoveries_ = 0;

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
  if (state_ == RunState::Venting && !ventingToTarget_) return;
  release();
}

bool VacuumSystem::running() const {
  return state_ == RunState::Pulling || state_ == RunState::Holding || ventingToTarget_;
}

bool VacuumSystem::takeDeadlineExpired() {
  const bool expired = deadlineExpired_;
  deadlineExpired_ = false;
  return expired;
}

bool VacuumSystem::released() const {
  return sensor_->present() && sensor_->readingValid() &&
         currentPressureKpa() <= VentFloorKpa + VentMarginKpa;
}

// Stop, then open: the pump never pulls against an open port. The state is VENT either way, and
// ventingToTarget_ is what tells the service loop which ending to wait for.
void VacuumSystem::beginTargetVent(bool deep) {
  pump_->off();
  vent_->open();
  ventingToTarget_ = true;
  ventLandingDeep_ = deep;
  ventStartMs_ = millis();
  setState(RunState::Venting);
}

// Seal the chamber and hand it back to the run. A vent that has just landed, and a target that is
// already where the pump works, both arrive here, so the headway the pump is judged on starts here
// rather than in the middle of a vent.
void VacuumSystem::closeValve() {
  vent_->close();
  ventingToTarget_ = false;
  bestDepthKpa_ = currentPressureKpa();
  progressMs_ = millis();
}

// The one decision, from the command's outcome, the state and the reading: a target of 0 releases,
// a reading past the band opens the valve, and anything else closes it and lets the loop work.
// Every command ends here, so the vent/start/stop choice is made once.
void VacuumSystem::converge() {
  const float measured = currentPressureKpa();

  // Ambient is the release, whichever way it was asked for.
  if (targetPressureKpa_ <= 0.0f) {
    if (state_ != RunState::Idle) release();
    return;
  }

  // Shallower than the chamber: the pump cannot vent, so the valve does, and the landing hands the
  // run back to the loop.
  if (measured > deepEdgeKpa(targetPressureKpa_)) {
    if (!ventingToTarget_) beginTargetVent(ControlVentToTarget);
    return;
  }

  closeValve();
  setState(measured >= shallowEdgeKpa(targetPressureKpa_) ? RunState::Holding : RunState::Pulling);
}

void VacuumSystem::setTarget(float pressureKpa) {
  // The bounds are the machine's, not a link's: 0 is ambient, and anything deeper than the
  // fitted floor is a depth the pump cannot reach. A refused value leaves the target unchanged.
  if (!(pressureKpa >= 0.0f) || pressureKpa > ControlFloorKpa) return;
  if (pressureKpa == targetPressureKpa_) return;
  targetPressureKpa_ = pressureKpa;

  // A target written while nothing is running is stored, not acted on.
  if (!running()) return;
  converge();
}

void VacuumSystem::resetFault() {
  faultCode_ = "";
  // The latch is what held the state, so clearing it has to put the machine somewhere: a
  // fault is cleared by releasing the chamber, which is where one leaves it anyway.
  if (state_ == RunState::Fault) release();
}

// The latch and the safe state, in that order: the pump is cut, the valve is opened, and the code
// is what `get` reports until resetFault(). Nothing drives the pump while it is set, because
// nothing but resetFault() leaves Fault.
void VacuumSystem::fail(const char* code) {
  pump_->off();
  vent_->open();  // drive the valve open: the safe direction
  ventingToTarget_ = false;
  faultCode_ = code ? code : "unknown";
  setState(RunState::Fault);
}

void VacuumSystem::service(uint32_t nowMs) {
  // A latched fault owns the state until it is reset: the vacuum system is already safe, and the
  // code is what `get` reports.
  if (faulted()) return;

  // Regulating on a reading nobody trusts is how the loop runs away, and a release cannot see the
  // floor without one. Only the resting state is allowed to be blind: it is vented already.
  if (state_ != RunState::Idle && (!sensor_->present() || !sensor_->readingValid())) {
    fail("sensor");
    return;
  }

  // A run has a deadline, and running() is what a run is: pulling, holding, or venting to a target
  // it was given. Expiry is a release, not a fault: the machine did what it was told for as long as
  // it was allowed to, and the run log names it.
  if (running() && holdMs_ != 0 && nowMs - runStartMs_ >= holdMs_) {
    deadlineExpired_ = true;
    release();
  }

  // VENT is one state with two endings. A target vent seals the chamber and hands it back to the
  // run; a release waits for this rig's vent floor and is a fault if it never gets there.
  if (state_ == RunState::Venting) {
    if (ventingToTarget_) {
      const float landing =
          ventLandingDeep_ ? targetPressureKpa_ + VentTargetLeadKpa : targetPressureKpa_;
      const bool arrived = currentPressureKpa() <= landing;
      const bool timedOut = (nowMs - ventStartMs_) >= VentTimeoutMs;
      if (!arrived && !timedOut) return;
      closeValve();
      setState(RunState::Pulling);
    } else {
      if (released()) {
        rest();
        return;
      }
      if ((nowMs - ventStartMs_) >= VentTimeoutMs) fail("vent_timeout");
      return;
    }
  }

  // The two transitions that are facts about the chamber rather than orders: the target has
  // been reached, and the release is over. A latched fault owns the state, so neither can
  // fire while one is held. Reached is the reading inside the band rather than merely past its
  // near edge: a vent lands past the target, and until the leak brings that back inside the band
  // the run is still travelling rather than holding.
  const float reached = currentPressureKpa();
  if (state_ == RunState::Pulling && reached >= shallowEdgeKpa(targetPressureKpa_) &&
      reached <= deepEdgeKpa(targetPressureKpa_)) {
    setState(RunState::Holding);
  }
  if (state_ == RunState::Pulling || state_ == RunState::Holding) {
    // A reading past the band is a chamber the pump was told to stop and did not, or a reading
    // nothing on the rig explains. The valve is the only way back, so the run vents to the target
    // and the pump takes it up again. A run that keeps needing that is the second case: the
    // reading never came back, so the run is released.
    const float measured = currentPressureKpa();
    if (measured < deepEdgeKpa(targetPressureKpa_)) {
      overshootReadings_ = 0;
    } else if (++overshootReadings_ >= ControlOvershootReadings) {
      overshootReadings_ = 0;
      if (++overshootRecoveries_ > ControlOvershootRecoveries) {
        fail("overshoot");
      } else {
        beginTargetVent(false);
        return;
      }
    }
    if (faulted()) return;
    regulate(nowMs);
  }
}

// The pump is a switch, so the loop is a relay rather than a regulator: on while the chamber is
// shallower than the lower edge of the band, off once the reading is back inside it, and the leak
// brings the depth down to the edge again. The one thing the loop must notice for itself is a pump
// that is on with no headway to show for it, which is a blocked hose or a pump that never started.
void VacuumSystem::regulate(uint32_t nowMs) {
  const float measured = currentPressureKpa();

  if (measured > shallowEdgeKpa(targetPressureKpa_)) {
    pump_->off();
    return;
  }

  // Headway is counted from the moment the pump came on, so a hold that cycles does not read as a
  // pull that never moved.
  if (!pump_->active()) {
    bestDepthKpa_ = measured;
    progressMs_ = nowMs;
  }
  pump_->on();

  if (measured >= bestDepthKpa_ + ControlProgressKpa) {
    bestDepthKpa_ = measured;
    progressMs_ = nowMs;
  } else if (nowMs - progressMs_ >= ControlProgressTimeoutMs) {
    fail("no_progress");
  }
}

}  // namespace gardener