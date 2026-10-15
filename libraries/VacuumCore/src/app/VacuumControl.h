#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "core/Snapshot.h"

namespace eggplan {

class VacuumSystem;

// The run: the target someone asked for, when to start and what a stop means, the fault latch
// that needs an explicit reset, and the countdown the controller pushes so the panel can show
// one. Nothing here keeps time.
//
// The plant underneath is VacuumSystem. This class decides only in terms of runs — has the
// target been reached, is this fault permanent — and asks the system everything about the
// chamber rather than answering it here. The run's duration belongs to the controller: the
// device holds until it is told to stop.
class VacuumControl {
 public:
  explicit VacuumControl(VacuumSystem& system) : system_(system) {}

  void update(uint32_t nowMs);

  // --- commands from the controller -----------------------------------------
  void setTarget(float kpa);
  void clearTarget();
  // The countdown the controller is running, in seconds, for the panel to draw. The device
  // never counts it and never acts on it; 0 means "show nothing".
  void setHoldSeconds(uint32_t seconds);
  bool startRun();  // false when there is no target, no reading, or a latched fault
  void stopRun();   // stop and release
  void clearFault();

  // --- state --------------------------------------------------------------
  RunState state() const { return state_; }
  uint32_t holdSeconds() const { return holdSeconds_; }

 private:
  void setState(RunState state);
  void beginVent();

  VacuumSystem& system_;

  RunState state_ = RunState::Idle;
  bool faultLatched_ = false;
  uint32_t holdSeconds_ = 0;
};

}  // namespace eggplan
