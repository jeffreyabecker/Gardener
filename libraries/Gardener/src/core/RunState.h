#pragma once

#include <stdint.h>

namespace gardener {
inline namespace core {

// What the device is doing, which is also the state a controller watches. There is no DONE: after a
// release it is IDLE, and the controller knows when the run is over because it is the one that
// ended it. HOLD is the target-reached edge, and it is the cue a controller times its hold from.
// VENT is the valve open to raise pressure: a release to ambient, or a correction to a shallower
// target, which returns to PULL/HOLD. There is no MANUAL (raw duty is not part of the interface,
// Q15).
//
// It is shared vocabulary rather than the control law's own: the wire prints its names, the overlay
// draws it and the published state carries it, so it sits below all three.
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

}  // namespace core
}  // namespace gardener
