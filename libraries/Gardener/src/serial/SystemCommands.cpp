#include "serial/SystemCommands.h"

#include <Arduino.h>
#include <utility>

#include "core/Formatting.h"
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
  println(io, "err bad_argument ", firstToken(token));
}

// The one line the wire adds to a set: which outcome the control reported, and how the wire says
// it. Every decision behind the answer — the bounds, the ambient target, the refusal — is the
// control's.
void set(Print& io, const String& args, SystemControl& control) {
  if (args.isEmpty()) {
    println(io, "err bad_argument");
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
      println(io, "ok target=", std::make_pair(pressureKpa, 2), " state=PULL");
      return;
    case ServeResult::Released:
      println(io, "ok target=0.0 state=VENT");
      return;
    case ServeResult::BelowRange:
      println(io, "err out_of_range field=target value=", quote(token), " min=0.0");
      return;
    case ServeResult::AboveRange:
      println(io, "err out_of_range field=target value=", quote(token), " max=", ControlFloorKpa);
      return;
    case ServeResult::NotReady:
      println(io, "err not_ready ", quote(control.refusal()));
      return;
  }
}

void vent(Print& io, const String& args, SystemControl& control) {
  if (!args.isEmpty()) {
    badArgument(io, args);
    return;
  }

  control.vent();
  println(io, "ok state=VENT");
}

void reset(Print& io, const String& args, SystemControl& control) {
  if (!args.isEmpty()) {
    badArgument(io, args);
    return;
  }

  control.reset();
  println(io, "ok state=IDLE");
}

}  // namespace

void SystemCommands::writeStatus(Print& io, const char* prefix) const {
  const SystemStatus status = control_.status();

  // 0 is the ambient target rather than "no target": the machine always has a setpoint, and 0 is
  // the one that holds the chamber released.
  println(io, prefix, "uptime_ms=", status.uptimeMs,
          " pressure_kpa=", std::make_pair(status.pressureKpa, 2),
          " target=", std::make_pair(status.targetKpa, 1), " state=", runStateName(status.state),
          " hold_time_ms=", status.holdTimeMs, " duty=", std::make_pair(status.dutyPercent, 1),
          " fault=", status.fault[0] ? status.fault : "none");
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
    println(io, "err bad_argument");
    return;
  }
  badArgument(io, word);
}

void SystemCommands::heartbeat(Print& io) { writeStatus(io, "hb "); }

}  // namespace gardener
