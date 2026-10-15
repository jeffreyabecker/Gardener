#include "app/VacuumControl.h"

#include "app/VacuumSystem.h"

namespace eggplan {

void VacuumControl::setState(RunState state) {
  if (state_ == state) return;
  state_ = state;
}

void VacuumControl::setTarget(float kpa) { system_.setTarget(kpa); }

void VacuumControl::clearTarget() {
  system_.clearTarget();
  // A run without a target is not a run, so clearing one releases the chamber.
  if (state_ == RunState::Pulling || state_ == RunState::Holding) {
    beginVent();
  } else if (state_ == RunState::Idle) {
    system_.rest();
  }
}

void VacuumControl::setHoldSeconds(uint32_t seconds) { holdSeconds_ = seconds; }

bool VacuumControl::startRun() {
  if (faultLatched_ || state_ == RunState::Fault) return false;
  if (!system_.targetActive()) return false;
  if (!system_.sensorOk() || !system_.readingValid()) return false;

  system_.start();
  // A countdown belongs to the run it was set for, so a new run starts without one.
  holdSeconds_ = 0;
  setState(RunState::Pulling);
  return true;
}

void VacuumControl::stopRun() {
  if (state_ == RunState::Idle) {
    system_.rest();
    return;
  }
  beginVent();
}

void VacuumControl::clearFault() {
  system_.resetFault();
  faultLatched_ = false;
  if (state_ == RunState::Fault) beginVent();
}

void VacuumControl::beginVent() {
  system_.vent();
  setState(RunState::Venting);
}

void VacuumControl::update(uint32_t nowMs) {
  // One entry point: the plant is serviced here and nowhere else, so the loop cannot be
  // stepped twice, or from two places.
  system_.service(nowMs);

  // The system detects a fault and makes the plant safe. That a fault is permanent until
  // someone resets it is a run rule, so the latch is here.
  if (system_.faulted()) {
    if (state_ != RunState::Fault) {
      faultLatched_ = true;
      setState(RunState::Fault);
    }
    return;
  }

  switch (state_) {
    case RunState::Idle:
    case RunState::Fault:
      return;

    case RunState::Pulling:
      if (system_.atTarget()) {
        // The one event the controller cannot work out for itself: the device's own deadband
        // has decided the target is reached, which is when the controller's countdown starts.
        setState(RunState::Holding);
      }
      return;

    case RunState::Holding:
      // The controller owns the hold's duration. The device holds, and waits to be told.
      return;

    case RunState::Venting:
      if (system_.vented()) {
        // The release is over, so the plant goes back to resting: it must not claim to be
        // venting while the run is idle.
        system_.rest();
        setState(RunState::Idle);
      }
      return;
  }
}

}  // namespace eggplan
