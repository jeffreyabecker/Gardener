#pragma once

#include <stdint.h>

namespace gardener {
class VacuumSystem;

// A write to the machine that the control task applies, carried across from the link or the VM
// that originated it. The op names the verb; `value` is the op's own — kPa for a target, whole
// seconds for a start's hold — so an op that has none leaves it at zero.
struct MachineCommand {
  enum class Op : uint8_t {
    Start,
    Stop,
    ResetFault,
    SetTarget,
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

}  // namespace gardener