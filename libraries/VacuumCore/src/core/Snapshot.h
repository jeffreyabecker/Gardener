#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/Config.h"

// Plain data shared between layers. The device is a serial peripheral, so this is the
// machine's read-only view: what a controller, the display or the log can see without
// holding a pointer into a module.
namespace gardener {

// What the device is doing, which is also the state a controller watches. There is no DONE:
// after venting it is IDLE, and the controller knows when the run is over because it is the one
// that ended it. HOLD is the target-reached edge, and it is the cue a controller times its hold
// from. There is no MANUAL (raw duty is not part of the interface, Q15).
enum class RunState : uint8_t { Idle, Pulling, Holding, Venting, Fault };

inline const char* runStateName(RunState state) {
  switch (state) {
    case RunState::Idle: return "IDLE";
    case RunState::Pulling: return "PULL";
    case RunState::Holding: return "HOLD";
    case RunState::Venting: return "VENT";
    case RunState::Fault: return "FAULT";
    default: return "?";
  }
}

struct VacuumState {
  RunState runState = RunState::Idle;
  // kpa is the sensor's own absolute reading. Every other pressure here is measured from the
  // captured reference, which is the frame the target is stated in.
  float kpa = 0.0f, vacuumKpa = 0.0f, vacuumPercent = 0.0f, ambientKpa = 0.0f;
  // Depth below the reference, and 0 while no target is set: setTarget() refuses anything that
  // shallow, so one number carries both facts and there is nothing to keep in step.
  float targetKpa = 0.0f;
  float duty = 0.0f;
  bool sensorOk = false;
  // Empty while nothing is latched, which is also how a latched fault is recognised.
  const char* faultCode = "";
};

struct SystemSnapshot {
  VacuumState vacuum;
  // battery: a readout only, nothing acts on it (Q11)
  bool batteryPresent = false;
  bool batteryLow = false;
  float batteryVolts = 0.0f;
  uint8_t batteryPercent = 0;
  // device
  uint32_t uptimeMs = 0;
};

}  // namespace gardener
