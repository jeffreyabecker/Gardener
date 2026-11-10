#pragma once

#include <stdint.h>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"

namespace gardener {
class Logger;

// The machine's beat: what a tick applies from the queue and how a burst of commands collapses, the
// order the parts are updated in, and the run log. A caller steps it with a millisecond, so the
// sequence a tick runs is reached by a case rather than only by the task that runs it; nothing here
// needs an RTOS, a clock of its own or a board.
//
// The queue is the composition's — the link posts into it — and nothing in a tick restarts the
// device: the machine is the chamber and the run, and the composition is the board.
class MachineTick {
 public:
  MachineTick(VacuumSystem& vacuum, Logger& logger, CommandQueue& queue);

  // The beat's start. The machine is brought up by the composition before the task is spawned, so
  // there is nothing to do here yet; it is here so every beat starts the same way.
  void begin() {}

  // One beat: applies what is queued, then updates the sensor and the pump, services the machine
  // and writes the run log, in that order. The millisecond is the tick's only clock.
  void tick(uint32_t nowMs);

 private:
  // One queued write, applied to the machine's own vacuum system.
  void apply(const MachineCommand& command);
  // A command the machine never had to apply, named with why, so a burst that collapses leaves the
  // same evidence as one that ran.
  void logCollapsed(const MachineCommand& command, const char* reason);
  // Every pending command, collapsed to what the machine should do rather than replayed step by
  // step: a stop discards everything queued before it, and repeated target writes reduce to the
  // last one.
  void drainCommands();
  // One line per state change, the time a vent took, the deadline and the fault code.
  void logRunChanges(uint32_t nowMs);

  VacuumSystem& vacuum_;
  Logger& logger_;
  CommandQueue& queue_;
  RunState lastState_;
  uint32_t ventStartMs_ = 0;
};

}  // namespace gardener