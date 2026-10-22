#include "MachineCommand.h"

#include "VacuumSystem.h"

namespace gardener {

bool applyMachineCommand(VacuumSystem& vacuum, const MachineCommand& command) {
  switch (command.op) {
    case MachineCommand::Op::Start: vacuum.start(); return true;
    case MachineCommand::Op::Stop: vacuum.stop(); return true;
    case MachineCommand::Op::ResetFault: vacuum.resetFault(); return true;
    case MachineCommand::Op::SetTarget: vacuum.setTarget(command.value); return true;
    case MachineCommand::Op::ClearTarget: vacuum.clearTarget(); return true;
    case MachineCommand::Op::Reboot: return false;
  }
  return false;
}

}  // namespace gardener
