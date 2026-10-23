#pragma once

#include <stdint.h>

namespace gardener {

class VacuumSystem;

// A write to the machine that the control task applies, carried across from the link or the VM
// that originated it. The op names the verb; `value` is the op's own — kPa for a target, whole
// seconds for a start's hold, the value for a setting — so an op that has none leaves it at zero.
struct MachineCommand {
  enum class Op : uint8_t {
    Start,
    Stop,
    ResetFault,
    SetTarget,
    SetFlow,
    SetVolume,
    SetFloor,
    SetPumpMax,
    SetDeadband,
    SetValve,
    Reboot,
  };
  Op op;
  float value = 0.0f;
};

// The op's name, for the run log and for a command the drain had to collapse.
const char* machineCommandName(MachineCommand::Op op);

// The commands the machine owns, applied in the order they were queued. False for an op the vacuum
// system cannot answer — Reboot, which belongs to the composition — so its caller handles that one.
bool applyMachineCommand(VacuumSystem& vacuum, const MachineCommand& command);

// The one way to ask the machine for something: the command joins the control task's queue and is
// applied on its next beat, so a writer never touches the vacuum system from its own thread. The
// queue handle and the task it feeds stay behind this call. False when the queue was full, in which
// case nothing was queued and the caller keeps the command.
bool postMachineCommand(const MachineCommand& command);

// A link's outbox: the commands it sent, in the order it sent them, with the ones a momentarily
// full queue could not take held until the link's next poll. The queue is still the only way into
// the machine, so a stop that arrives behind a full queue is delayed by a beat rather than lost.
class CommandOutbox {
 public:
  // Hands the command to the control task, or holds it when the queue is full. True when it was
  // queued; false when it is waiting for flush().
  bool post(const MachineCommand& command);

  // Retries what is held, oldest first, and stops at the first that still does not fit so the
  // order the link sent is kept.
  void flush();

 private:
  // Deep enough for a burst a link can produce between two polls; a link that writes faster than
  // the control task drains is the one case this cannot hold, and it drops the newest rather than
  // blocking the link.
  static constexpr uint8_t Capacity = 8;
  MachineCommand held_[Capacity] = {};
  uint8_t head_ = 0;
  uint8_t count_ = 0;
};

}  // namespace gardener
