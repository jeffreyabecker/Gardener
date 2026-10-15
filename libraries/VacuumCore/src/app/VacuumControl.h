#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "core/Snapshot.h"

namespace eggplan {

class VacuumSystem;

// The run: the target someone asked for, the hold timer, when to start and what a stop
// means, and the fault latch that needs an explicit reset.
//
// The plant underneath is VacuumSystem. This class decides only in terms of runs — has the
// target been reached, has the countdown expired, is this fault permanent — and asks the
// system everything about the chamber rather than answering it here.
class VacuumControl {
 public:
  explicit VacuumControl(VacuumSystem& system) : system_(system) {}

  void update(uint32_t nowMs);

  // --- commands from the controller -----------------------------------------
  void setTarget(float kpa);
  void clearTarget();
  void setDurationSeconds(uint32_t seconds);
  bool startRun();  // false when there is no target, no reading, or a latched fault
  void stopRun();   // stop and release
  void clearFault();

  // --- state --------------------------------------------------------------
  RunState state() const { return state_; }
  uint32_t durationSeconds() const { return durationSeconds_; }
  uint32_t remainingSeconds() const { return remainingSeconds_; }

  // Delegates, kept so the readers can be retargeted in one step rather than two.
  bool targetActive() const;
  float targetKpa() const;
  float minReachableKpa() const;

 private:
  void setState(RunState state);
  void beginVent();

  VacuumSystem& system_;

  RunState state_ = RunState::Idle;
  bool faultLatched_ = false;
  uint32_t durationSeconds_ = 0;
  uint32_t remainingSeconds_ = 0;
  uint32_t countdownEndMs_ = 0;
};

}  // namespace eggplan
