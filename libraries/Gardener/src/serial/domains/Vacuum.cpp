#include <Arduino.h>
#include <stdint.h>

#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "machine/VacuumSystem.h"
#include "serial/Domain.h"
#include "serial/Domains.h"

namespace gardener {
inline namespace serial {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

class VacuumDomain : public Domain {
 public:
  VacuumDomain(Machine& app, CommandOutbox& outbox) : Domain(outbox), app_(app) {}

  const char* name() const override { return "vacuum"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io);
  void target(Stream& io, const String& args);
  void start(Stream& io, const String& args);
  void stop(Stream& io, const String& args);
  void reset(Stream& io, const String& args);

  Machine& app_;
};

void VacuumDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > COMMAND_LINE_MAX) {
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
    get(io);
    return;
  }
  if (verb == "target") {
    target(io, args);
    return;
  }
  if (verb == "start") {
    start(io, args);
    return;
  }
  if (verb == "stop") {
    stop(io, args);
    return;
  }
  if (verb == "reset") {
    reset(io, args);
    return;
  }

  io.print("err unknown_verb ");
  io.print(verb);
  io.print('\n');
}

void VacuumDomain::get(Stream& io) {
  VacuumSystem& vacuum = app_.vacuum();
  io.print("ok pressure_kpa=");
  io.print(vacuum.currentPressureKpa(), 2u);

  // 0 is the ambient target rather than "no target": the machine always has a setpoint, and 0 is
  // the one that holds the chamber released.
  io.print(" target=");
  io.print(vacuum.targetPressureKpa(), 1u);

  io.print(" floor=");
  io.print(CONTROL_FLOOR_KPA, 1u);
  io.print(" duty=");
  io.print(vacuum.pump().dutyPercent(), 1u);
  io.print(" state=");
  io.print(runStateName(vacuum.runState()));
  io.print(" sensor=");
  io.print(vacuum.sensor().present() ? "1" : "0");
  io.print(" fault=");
  const char* fault = vacuum.faultCode();
  io.print(fault[0] ? fault : "none");
  io.print(" hold=");
  io.print(vacuum.running() ? vacuum.holdSeconds() : 0u);
  io.print(" max_run=");
  io.print((uint32_t)(CONTROL_MAX_RUN_MS / 1000));
  io.print(" up=");
  io.print(app_.uptimeMs());
  io.print('\n');
}

void VacuumDomain::target(Stream& io, const String& args) {
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
  if (pressureKpa > CONTROL_FLOOR_KPA) {
    io.print("err out_of_range field=target value=");
    io.print(token);
    io.print(" max=");
    io.print(CONTROL_FLOOR_KPA, 1u);
    io.print('\n');
    return;
  }

  const MachineCommand set{MachineCommand::Op::SetTarget, pressureKpa};
  outbox_.post(set);
  io.print("ok target=");
  io.print(pressureKpa, 1u);
  io.print('\n');
}

void VacuumDomain::start(Stream& io, const String& args) {
  // The optional argument is the hold, in whole seconds. Without one the run gets the compiled
  // maximum, which is also its ceiling.
  const uint32_t maxSeconds = CONTROL_MAX_RUN_MS / 1000;
  uint32_t holdSeconds = 0;
  if (!args.isEmpty()) {
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

    const float seconds = token.toFloat();
    if (seconds > (float)maxSeconds) {
      io.print("err out_of_range field=hold value=");
      io.print(token);
      io.print(" max=");
      io.print(maxSeconds);
      io.print('\n');
      return;
    }
    if (!(seconds >= 1.0f) || seconds != (float)(uint32_t)seconds) {
      io.print("err out_of_range field=hold value=");
      io.print(token);
      io.print(" min=1\n");
      return;
    }
    holdSeconds = (uint32_t)seconds;
  }

  // The refusal is a pure read of the machine's fault and reading, so it is answered from a dirty
  // read; a would-succeed start is queued and the reply is optimistic.
  const char* refusal = app_.vacuum().startRefusal();
  if (refusal != nullptr) {
    io.print("err not_ready ");
    io.print(refusal);
    io.print('\n');
    return;
  }
  const MachineCommand start{MachineCommand::Op::Start, (float)holdSeconds};
  outbox_.post(start);
  io.print("ok state=PULL\n");
}

void VacuumDomain::stop(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  const MachineCommand stop{MachineCommand::Op::Stop};
  outbox_.post(stop);
  io.print("ok state=VENT\n");
}

void VacuumDomain::reset(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  const MachineCommand reset{MachineCommand::Op::ResetFault};
  outbox_.post(reset);
  io.print("ok state=IDLE\n");
}

}  // namespace

Domain* createVacuumDomain(Machine& app, CommandOutbox& outbox) {
  return new VacuumDomain(app, outbox);
}

}  // namespace serial
}  // namespace gardener
