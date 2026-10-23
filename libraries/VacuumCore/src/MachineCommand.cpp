#include "MachineCommand.h"

#include "Machine.h"
#include "VacuumSystem.h"
#include "storage/MachineSettings.h"

namespace gardener {

const char* machineCommandName(MachineCommand::Op op) {
  switch (op) {
    case MachineCommand::Op::Start: return "start";
    case MachineCommand::Op::Stop: return "stop";
    case MachineCommand::Op::ResetFault: return "reset";
    case MachineCommand::Op::SetTarget: return "target";
    case MachineCommand::Op::SetFlow: return "flow";
    case MachineCommand::Op::SetVolume: return "volume";
    case MachineCommand::Op::SetFloor: return "floor";
    case MachineCommand::Op::SetPumpMax: return "pump_max";
    case MachineCommand::Op::SetDeadband: return "deadband";
    case MachineCommand::Op::SetValve: return "valve";
    case MachineCommand::Op::Reboot: return "reboot";
  }
  return "?";
}

bool applyMachineCommand(VacuumSystem& vacuum, const MachineCommand& command) {
  MachineSettings& settings = vacuum.settings();
  switch (command.op) {
    case MachineCommand::Op::Start: vacuum.start((uint32_t)command.value); return true;
    case MachineCommand::Op::Stop: vacuum.stop(); return true;
    case MachineCommand::Op::ResetFault: vacuum.resetFault(); return true;
    case MachineCommand::Op::SetTarget: vacuum.setTarget(command.value); return true;
    case MachineCommand::Op::SetFlow: settings.setPumpFlowLpm(command.value); return true;
    case MachineCommand::Op::SetVolume: settings.setChamberVolumeL(command.value); return true;
    case MachineCommand::Op::SetFloor: settings.setFloorPressureKpa(command.value); return true;
    case MachineCommand::Op::SetPumpMax: settings.setPumpMaxPercent(command.value); return true;
    case MachineCommand::Op::SetDeadband:
      settings.setDeadbandPressureKpa(command.value);
      return true;
    case MachineCommand::Op::SetValve:
      settings.setValveNormallyOpen(command.value != 0.0f);
      return true;
    case MachineCommand::Op::Reboot: return false;
  }
  return false;
}

bool CommandOutbox::post(const MachineCommand& command) {
  if (count_ == 0 && postMachineCommand(command)) return true;
  if (count_ == Capacity) return false;  // the newest is what a link cannot hold
  held_[(head_ + count_) % Capacity] = command;
  count_++;
  return false;
}

void CommandOutbox::flush() {
  while (count_ > 0) {
    if (!postMachineCommand(held_[head_])) return;
    head_ = (head_ + 1) % Capacity;
    count_--;
  }
}

}  // namespace gardener
