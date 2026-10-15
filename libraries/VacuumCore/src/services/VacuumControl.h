#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "core/Pid.h"
#include "core/Snapshot.h"

namespace eggplan {

class PressureSensor;
class Pump;
class VentValve;

// The pressure loop, the run timer and the valve: everything the device has to own to
// be fast or safe.
//
// The controller owns the target, the timer, and when to start and stop. The device owns
// what happens when nobody is watching: pulling to the target, expiring the countdown,
// and releasing the chamber through one path on stop, abort, link loss or fault.
class VacuumControl {
 public:
  VacuumControl(Pump& pump, PressureSensor& sensor, VentValve& vent, const PidConfig& pid)
      : pump_(pump), sensor_(sensor), vent_(vent), pid_(pid) {}

  void begin();
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
  bool targetActive() const { return targetActive_; }
  float targetKpa() const { return targetKpa_; }
  uint32_t durationSeconds() const { return durationSeconds_; }
  uint32_t remainingSeconds() const { return remainingSeconds_; }

  // One shot: the run finished and the chamber was released. Nothing consumes it today
  // beyond the log, but a repeat-run feature or a notification would.
  bool consumeCompleted();

  // Deepest target the pump can actually reach: ambient minus its suction rating,
  // floored by the sanity limit.
  float minReachableKpa() const;

 private:
  void setState(RunState state);
  void beginVent();
  float step(uint32_t nowMs);
  void fault(const char* code, const char* text);

  Pump& pump_;
  PressureSensor& sensor_;
  VentValve& vent_;
  Pid pid_;

  RunState state_ = RunState::Idle;
  bool targetActive_ = false;
  bool faultLatched_ = false;
  bool completed_ = false;
  float targetKpa_ = 0.0f;
  uint32_t durationSeconds_ = 0;
  uint32_t remainingSeconds_ = 0;
  uint32_t countdownEndMs_ = 0;
  uint32_t ventStartMs_ = 0;
  float lastError_ = 0.0f;
  float lastMeasured_ = 0.0f;
  float output_ = 0.0f;
  uint32_t lastControlMs_ = 0;
  uint32_t lastDerivativeMs_ = 0;
};

}  // namespace eggplan
