#include <Arduino.h>
#include <stdint.h>

#include "machine/Machine.h"
#include "machine/SystemControl.h"
#include "serial/SerialLink.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

void get(Stream& io, const SystemControl& control) {
  const SystemStatus status = control.status();
  io.print("ok pressure_kpa=");
  io.print(status.pressureKpa, 2u);

  // 0 is the ambient target rather than "no target": the machine always has a setpoint, and 0 is
  // the one that holds the chamber released.
  io.print(" target=");
  io.print(status.targetKpa, 1u);

  io.print(" state=");
  io.print(runStateName(status.state));
  io.print(" duty=");
  io.print(status.dutyPercent, 1u);
  io.print(" fault=");
  io.print(status.fault[0] ? status.fault : "none");
  io.print('\n');
}

// The one line the wire adds to a set: which outcome the control reported, and how the wire says
// it. Every decision behind the answer — the bounds, the ambient target, the refusal — is the
// control's.
void set(Stream& io, const String& args, SystemControl& control) {
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

  switch (control.set(pressureKpa)) {
    case ServeResult::Ok:
      io.print("ok target=");
      io.print(pressureKpa, 1u);
      io.print(" state=PULL\n");
      return;
    case ServeResult::Released:
      io.print("ok target=0.0 state=VENT\n");
      return;
    case ServeResult::BelowRange:
      io.print("err out_of_range field=target value=");
      io.print(token);
      io.print(" min=0.0\n");
      return;
    case ServeResult::AboveRange:
      io.print("err out_of_range field=target value=");
      io.print(token);
      io.print(" max=");
      io.print(ControlFloorKpa, 1u);
      io.print('\n');
      return;
    case ServeResult::NotReady:
      io.print("err not_ready ");
      io.print(control.refusal());
      io.print('\n');
      return;
  }
}

void vent(Stream& io, const String& args, SystemControl& control) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  control.vent();
  io.print("ok state=VENT\n");
}

void reset(Stream& io, const String& args, SystemControl& control) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  control.reset();
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
    get(io, control_);
    return;
  }
  if (verb == "set") {
    set(io, args, control_);
    return;
  }
  if (verb == "vent") {
    vent(io, args, control_);
    return;
  }
  if (verb == "reset") {
    reset(io, args, control_);
    return;
  }

  io.print("err unknown_verb ");
  io.print(verb);
  io.print('\n');
}

}  // namespace gardener
