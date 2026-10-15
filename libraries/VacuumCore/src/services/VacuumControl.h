#pragma once

#include <stdint.h>

#include "core/Config.h"
#include "core/Pid.h"
#include "core/Snapshot.h"

namespace eggplan {

class PressureSensor;
class Pump;

// Closed loop vacuum control.
//
// The pump only pulls down, so the loop can never vent: when the measured pressure
// undershoots the target the output goes to zero and the chamber bleeds back up.
class VacuumControl {
 public:
  VacuumControl(Pump& pump, PressureSensor& sensor, const PidConfig& pid)
      : pump_(pump), sensor_(sensor), pid_(pid) {}

  void begin();
  void update(uint32_t nowMs);

  void setTarget(float kpa);
  void disableTarget();
  void setManualDuty(float percent);
  void stop();
  void clearFault();

  ControlState state() const { return state_; }
  bool targetActive() const { return targetActive_; }
  float targetKpa() const { return targetKpa_; }
  float error() const { return lastError_; }
  float output() const { return output_; }

  // Deepest target the pump can actually reach: ambient minus its suction rating,
  // floored by the sanity limit.
  float minReachableKpa() const;

 private:
  void setState(ControlState state);

  Pump& pump_;
  PressureSensor& sensor_;
  Pid pid_;

  ControlState state_ = ControlState::Idle;
  bool targetActive_ = false;
  bool fault_ = false;
  float targetKpa_ = 0.0f;
  float lastError_ = 0.0f;
  float lastMeasured_ = 0.0f;
  float output_ = 0.0f;
  uint32_t lastControlMs_ = 0;
  uint32_t lastDerivativeMs_ = 0;
};

}  // namespace eggplan
