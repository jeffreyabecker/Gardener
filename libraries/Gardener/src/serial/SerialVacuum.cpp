#include <Arduino.h>
#include <stdint.h>

#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"
#include "serial/SerialLink.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

void get(Stream& io, VacuumSystem& vacuum) {
  io.print("ok pressure_kpa=");
  io.print(vacuum.currentPressureKpa(), 2u);

  // 0 is the ambient target rather than "no target": the machine always has a setpoint, and 0 is
  // the one that holds the chamber released.
  io.print(" target=");
  io.print(vacuum.targetPressureKpa(), 1u);

  io.print(" state=");
  io.print(runStateName(vacuum.runState()));
  io.print(" duty=");
  io.print(vacuum.pump().dutyPercent(), 1u);
  io.print(" fault=");
  const char* fault = vacuum.faultCode();
  io.print(fault[0] ? fault : "none");
  io.print('\n');
}

// A set is a target and a run in one word, so the two refusals are the machine's own: the bounds the
// loop enforces, and the reason a start would not take. A refused set queues nothing.
void set(Stream& io, const String& args, Machine& machine, CommandQueue& queue) {
  if (args.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }

  const int space = args.indexOf(' ');
  const String token = space < 0 ? args : args.substring(0, (unsigned int)space);
  String extra = space < 0 ? String() : args.substring((unsigned int)space + 1);
  extra.trim();
  if (!extra.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(extra));
    io.print('\n');
    return;
  }

  // `-` stays as the shorthand for the ambient target, which is what 0 means as well.
  float pressureKpa = 0.0f;
  if (token != "-") pressureKpa = token.toFloat();

  // `!(>=)` rather than `<` so a token `toFloat` reads as NaN is refused with the rest.
  if (!(pressureKpa >= 0.0f)) {
    io.print("err out_of_range field=target value=");
    io.print(token);
    io.print(" min=0.0\n");
    return;
  }
  if (pressureKpa > ControlFloorKpa) {
    io.print("err out_of_range field=target value=");
    io.print(token);
    io.print(" max=");
    io.print(ControlFloorKpa, 1u);
    io.print('\n');
    return;
  }

  // Aiming at ambient and running it is the vent, and a vent needs no reading: the ambient target
  // is the release however it was asked for.
  if (pressureKpa <= 0.0f) {
    const MachineCommand ambient{MachineCommand::Op::SetTarget, 0.0f};
    queue.post(ambient);
    const MachineCommand stop{MachineCommand::Op::Stop};
    queue.post(stop);
    io.print("ok target=0.0 state=VENT\n");
    return;
  }

  // The refusal is a pure read of the machine's fault and reading, so it is answered from a dirty
  // read; a would-succeed set is queued and the reply is optimistic.
  const char* refusal = machine.vacuum().startRefusal();
  if (refusal != nullptr) {
    io.print("err not_ready ");
    io.print(refusal);
    io.print('\n');
    return;
  }

  // The hold is the machine's own ceiling: a wire that asks for no duration gets the longest run the
  // machine will grant, and no document is read on either side of it.
  const MachineCommand target{MachineCommand::Op::SetTarget, pressureKpa};
  queue.post(target);
  const MachineCommand start{MachineCommand::Op::Start, 0.0f};
  queue.post(start);
  io.print("ok target=");
  io.print(pressureKpa, 1u);
  io.print(" state=PULL\n");
}

void vent(Stream& io, const String& args, CommandQueue& queue) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  const MachineCommand stop{MachineCommand::Op::Stop};
  queue.post(stop);
  io.print("ok state=VENT\n");
}

void reset(Stream& io, const String& args, CommandQueue& queue) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  const MachineCommand reset{MachineCommand::Op::ResetFault};
  queue.post(reset);
  io.print("ok state=IDLE\n");
}

}  // namespace

// The machine's own commands: a target and the run it asks for, the release, and the latch's reset.
void SerialLink::executeVacuum(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > CommandLineMax) {
    io.print("err bad_argument\n");
    return;
  }

  const int space = line.indexOf(' ');
  String verb = space < 0 ? line : line.substring(0, (unsigned int)space);
  String args = space < 0 ? String() : line.substring((unsigned int)space + 1);
  args.trim();
  verb.toLowerCase();

  if (verb.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }
  if (verb == "get") {
    if (!args.isEmpty()) {
      io.print("err bad_argument ");
      io.print(firstToken(args));
      io.print('\n');
      return;
    }
    get(io, parts_.machine.vacuum());
    return;
  }
  if (verb == "set") {
    set(io, args, parts_.machine, queue_);
    return;
  }
  if (verb == "vent") {
    vent(io, args, queue_);
    return;
  }
  if (verb == "reset") {
    reset(io, args, queue_);
    return;
  }

  io.print("err unknown_verb ");
  io.print(verb);
  io.print('\n');
}

}  // namespace gardener
