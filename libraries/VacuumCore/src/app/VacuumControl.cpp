#include "app/VacuumControl.h"

#include <Arduino.h>

#include "app/VacuumSystem.h"

namespace eggplan {

void VacuumControl::setState(RunState state) {
  if (state_ == state) return;
  state_ = state;
}

void VacuumControl::setTarget(float kpa) { system_.setTarget(kpa); }

void VacuumControl::clearTarget() {
  system_.clearTarget();
  remainingSeconds_ = 0;
  // A run without a target is not a run, so clearing one releases the chamber.
  if (state_ == RunState::Pulling || state_ == RunState::Holding) {
    beginVent();
  } else if (state_ == RunState::Idle) {
    system_.rest();
  }
}

void VacuumControl::setDurationSeconds(uint32_t seconds) {
  durationSeconds_ = seconds;
  if (state_ == RunState::Holding) {
    // A timer set while holding counts from now.
    countdownEndMs_ = millis() + seconds * 1000UL;
    remainingSeconds_ = seconds;
  }
}

bool VacuumControl::startRun() {
  if (faultLatched_ || state_ == RunState::Fault) return false;
  if (!system_.targetActive()) return false;
  if (!system_.sensorOk() || !system_.readingValid()) return false;

  system_.start();
  remainingSeconds_ = durationSeconds_;
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
      remainingSeconds_ = 0;
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
        // The countdown starts here (Q1), so the number means "hold time remaining".
        countdownEndMs_ = nowMs + durationSeconds_ * 1000UL;
        remainingSeconds_ = durationSeconds_;
        setState(RunState::Holding);
      }
      return;

    case RunState::Holding: {
      if (durationSeconds_ == 0) {
        remainingSeconds_ = 0;
        return;
      }
      uint32_t leftMs = countdownEndMs_ > nowMs ? countdownEndMs_ - nowMs : 0;
      remainingSeconds_ = (leftMs + 999) / 1000;
      if (leftMs == 0) {
        beginVent();
      }
      return;
    }

    case RunState::Venting:
      if (system_.vented()) {
        // The release is over, so the plant goes back to resting: it must not claim to be
        // venting while the run is idle.
        system_.rest();
        remainingSeconds_ = 0;
        setState(RunState::Idle);
      }
      return;
  }
}

}  // namespace eggplan
