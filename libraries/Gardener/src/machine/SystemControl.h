#pragma once

#include <stdint.h>

#include "core/RunState.h"

namespace gardener {
class CommandQueue;
class VacuumSystem;

// What a front end can ask the device to do, and the one way to ask it. A peer — the serial link
// today, anything beside it later — holds one of these instead of the vacuum system and the command
// queue, so the bounds, the ambient target and the two commands a set is made of live in one place
// rather than once per link.
//
// The answer names what happened, not how to say it: the reply's words, its fields and its decimals
// belong to the wire.
enum class ServeResult : uint8_t {
  Ok,          // a target and a run were queued
  Released,    // the ambient target: a vent was queued instead
  BelowRange,  // a target under ambient, which is what a token that is not a number reads as
  AboveRange,  // deeper than ControlFloorKpa
  NotReady,    // a latched fault or no valid reading: nothing was queued
};

// The machine's reading, copied once so a reply reports one instant.
struct SystemStatus {
  float pressureKpa = 0.0f;
  float targetKpa = 0.0f;
  RunState state = RunState::Idle;
  float dutyPercent = 0.0f;
  const char* fault = "";  // empty when nothing is latched
};

class SystemControl {
 public:
  SystemControl(VacuumSystem& vacuum, CommandQueue& queue);

  SystemStatus status() const;

  ServeResult set(float targetKpa);
  ServeResult vent();
  ServeResult reset();

  // Why a set would answer NotReady, or nullptr when it would take one.
  const char* refusal() const;

 private:
  VacuumSystem& vacuum_;
  CommandQueue& queue_;
};

}  // namespace gardener
