#pragma once

#include <stdint.h>

namespace gardener {

class VacuumSystem;

// A write to the machine that the control task applies, carried across from the link or the VM
// that originated it. The op names the verb; the fields are the op's own, so a field that does
// not belong to the op is ignored.
struct MachineCommand {
  enum class Op : uint8_t { Start, Stop, ResetFault, SetTarget, ClearTarget, Reboot };
  Op op;
  float value = 0.0f;  // SetTarget: kPa below ambient
};

// The commands the machine owns, applied in the order they were queued. False for an op the vacuum
// system cannot answer — Reboot, which belongs to the composition — so its caller handles that one.
bool applyMachineCommand(VacuumSystem& vacuum, const MachineCommand& command);

// The one way to ask the machine for something: the command joins the control task's queue and is
// applied on its next beat, so a writer never touches the vacuum system from its own thread. The
// queue handle and the task it feeds stay behind this call.
void postMachineCommand(const MachineCommand& command);

}  // namespace gardener
