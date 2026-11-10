#include "machine/MachineCommand.h"

#include "machine/Machine.h"
#include "machine/VacuumSystem.h"

namespace gardener {

const char* machineCommandName(MachineCommand::Op op) {
  switch (op) {
    case MachineCommand::Op::Start:
      return "start";
    case MachineCommand::Op::Stop:
      return "stop";
    case MachineCommand::Op::ResetFault:
      return "reset";
    case MachineCommand::Op::SetTarget:
      return "target";
  }
  return "?";
}

void applyMachineCommand(VacuumSystem& vacuum, const MachineCommand& command) {
  switch (command.op) {
    case MachineCommand::Op::Start:
      vacuum.start((uint32_t)command.value);
      return;
    case MachineCommand::Op::Stop:
      vacuum.stop();
      return;
    case MachineCommand::Op::ResetFault:
      vacuum.resetFault();
      return;
    case MachineCommand::Op::SetTarget:
      vacuum.setTarget(command.value);
      return;
  }
}

}  // namespace gardener