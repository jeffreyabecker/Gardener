#pragma once

#include <stdint.h>

#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"

namespace gardener {
inline namespace core {
class Logger;
struct SystemState;
}

inline namespace beats {

// The machine's beat: what a tick applies from the queue and how a burst of commands collapses, the
// order the parts are updated in, and the run log. A caller steps it with a millisecond, so the
// sequence a tick runs is reached by a case rather than only by the task that runs it; nothing here
// needs an RTOS, a clock of its own or a board.
//
// The queue is the composition's — the links and the panel post into the same one — and a Reboot is
// not applied here: it belongs to the machine's composition, so takeReboot() hands it back rather
// than restarting from inside a tick.
class MachineTick {
 public:
  MachineTick(VacuumSystem& vacuum, Logger& logger, CommandQueue& queue, SystemState& state);

  // One beat: applies what is queued, then updates the sensor and the pump, services the machine
  // and writes the run log, in that order. The millisecond is the tick's only clock.
  void tick(uint32_t nowMs);

  // True once after a queued Reboot arrived on a tick, so the composition can perform it.
  bool takeReboot();

 private:
  // One queued write. The vacuum system is the machine's own; the reboot is the composition's, so
  // it is parked for takeReboot().
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
  // The machine's slice of the published state, once at the end of the beat.
  void publish();

  VacuumSystem& vacuum_;
  Logger& logger_;
  CommandQueue& queue_;
  // The machine's slice of the published state, written at the end of each beat.
  SystemState& state_;
  RunState lastState_;
  uint32_t ventStartMs_ = 0;
  bool rebootPending_ = false;
};

}  // namespace beats
}  // namespace gardener
