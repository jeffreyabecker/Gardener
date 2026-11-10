#include "machine/SystemControl.h"

#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"

namespace gardener {

SystemControl::SystemControl(VacuumSystem& vacuum, CommandQueue& queue)
    : vacuum_(vacuum), queue_(queue) {}

SystemStatus SystemControl::status() const {
  SystemStatus status;
  status.pressureKpa = vacuum_.currentPressureKpa();
  status.targetKpa = vacuum_.targetPressureKpa();
  status.state = vacuum_.runState();
  status.dutyPercent = vacuum_.dutyPercent();
  status.fault = vacuum_.faultCode();
  return status;
}

// A set is a target and a run in one word, so both refusals are the machine's own: the bounds the
// loop enforces, and the reason a start would not take. A refused set queues nothing.
ServeResult SystemControl::set(float targetKpa) {
  // `!(>=)` rather than `<` so a token `toFloat` reads as NaN is refused with the rest.
  if (!(targetKpa >= 0.0f)) return ServeResult::BelowRange;
  if (targetKpa > ControlFloorKpa) return ServeResult::AboveRange;

  // Aiming at ambient and running it is the vent, and a vent needs no reading: the ambient target
  // is the release however it was asked for.
  if (targetKpa <= 0.0f) {
    queue_.post(MachineCommand{MachineCommand::Op::SetTarget, 0.0f});
    queue_.post(MachineCommand{MachineCommand::Op::Stop});
    return ServeResult::Released;
  }

  // The refusal is a pure read of the machine's fault and reading, so it is answered from a dirty
  // read; a would-succeed set is queued and the reply is optimistic.
  if (vacuum_.startRefusal() != nullptr) return ServeResult::NotReady;

  // The hold is the machine's own ceiling: a set names no duration, and 0 is how a start asks for
  // the longest run the machine will grant.
  queue_.post(MachineCommand{MachineCommand::Op::SetTarget, targetKpa});
  queue_.post(MachineCommand{MachineCommand::Op::Start, 0.0f});
  return ServeResult::Ok;
}

// The release, and the latch, are one command each and cannot be refused.
ServeResult SystemControl::vent() {
  queue_.post(MachineCommand{MachineCommand::Op::Stop});
  return ServeResult::Ok;
}

ServeResult SystemControl::reset() {
  queue_.post(MachineCommand{MachineCommand::Op::ResetFault});
  return ServeResult::Ok;
}

const char* SystemControl::refusal() const { return vacuum_.startRefusal(); }

}  // namespace gardener
