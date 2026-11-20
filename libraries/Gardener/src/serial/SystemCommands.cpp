#include "serial/SystemCommands.h"

#include <Arduino.h>

#include "machine/Machine.h"
#include "machine/SystemControl.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

// An argument the word does not take is refused by name, the way every line under a prefix is.
void badArgument(Print& io, const String& token) {
  io.print("err bad_argument ");
  io.print(firstToken(token));
  io.print('\n');
}

// The one line the wire adds to a set: which outcome the control reported, and how the wire says
// it. Every decision behind the answer — the bounds, the ambient target, the refusal — is the
// control's.
void set(Print& io, const String& args, SystemControl& control) {
  if (args.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }

  const int space = args.indexOf(' ');
  const String token = space < 0 ? args : args.substring(0, (unsigned int)space);
  String extra = space < 0 ? String() : args.substring((unsigned int)space + 1);
  extra.trim();
  if (!extra.isEmpty()) {
    badArgument(io, extra);
    return;
  }

  // `-` stays as the shorthand for the ambient target, which is what 0 means as well.
  float pressureKpa = 0.0f;
  if (token != "-") pressureKpa = token.toFloat();

  switch (control.set(pressureKpa)) {
    case ServeResult::Ok:
      io.print("ok target=");
      io.print(pressureKpa, 2u);
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

void vent(Print& io, const String& args, SystemControl& control) {
  if (!args.isEmpty()) {
    badArgument(io, args);
    return;
  }

  control.vent();
  io.print("ok state=VENT\n");
}

void reset(Print& io, const String& args, SystemControl& control) {
  if (!args.isEmpty()) {
    badArgument(io, args);
    return;
  }

  control.reset();
  io.print("ok state=IDLE\n");
}

}  // namespace

void SystemCommands::writeStatus(Print& io, const char* prefix) const {
  const SystemStatus status = control_.status();

  String line = prefix;
  line += "uptime_ms=";
  line += String(status.uptimeMs);
  line += " pressure_kpa=";
  line += String(status.pressureKpa, 2u);

  // 0 is the ambient target rather than "no target": the machine always has a setpoint, and 0 is
  // the one that holds the chamber released.
  line += " target=";
  line += String(status.targetKpa, 1u);

  line += " state=";
  line += runStateName(status.state);
  line += " duty=";
  line += String(status.dutyPercent, 1u);
  line += " fault=";
  line += (status.fault[0] ? status.fault : "none");
  line += '\n';
  io.print(line);
}

// The machine's own commands: a target and the run it asks for, the release, and the latch's reset.
void SystemCommands::serve(Print& io, const String& args) {
  const int space = args.indexOf(' ');
  String word = space < 0 ? args : args.substring(0, (unsigned int)space);
  String rest = space < 0 ? String() : args.substring((unsigned int)space + 1);
  rest.trim();
  word.toLowerCase();

  if (word == "get") {
    if (!rest.isEmpty()) {
      badArgument(io, rest);
      return;
    }
    writeStatus(io, "ok ");
    return;
  }
  if (word == "set") {
    set(io, rest, control_);
    return;
  }
  if (word == "vent") {
    vent(io, rest, control_);
    return;
  }
  if (word == "reset") {
    reset(io, rest, control_);
    return;
  }

  // A bare `sys` is nothing the machine answers: `sys get` is the one way to ask for the status.
  if (word.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }
  badArgument(io, word);
}

void SystemCommands::heartbeat(Print& io) { writeStatus(io, "hb "); }

}  // namespace gardener
