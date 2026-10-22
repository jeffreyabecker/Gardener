#include <Arduino.h>
#include <stdint.h>

#include "Machine.h"
#include "Tasks.h"
#include "VacuumSystem.h"
#include "serial/Domain.h"
#include "serial/Domains.h"
#include "serial/Gate.h"
#include "core/Config.h"
#include "core/Parse.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

class VacuumDomain : public Domain {
 public:
  explicit VacuumDomain(Machine& app) : app_(app) {}

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
  if (line.length() > cfg::COMMAND_LINE_MAX) {
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
    if (!requireControl(app_, io)) return;
    target(io, args);
    return;
  }
  if (verb == "start") {
    if (!requireControl(app_, io)) return;
    start(io, args);
    return;
  }
  if (verb == "stop") {
    if (!requireControl(app_, io)) return;
    stop(io, args);
    return;
  }
  if (verb == "reset") {
    if (!requireControl(app_, io)) return;
    reset(io, args);
    return;
  }

  io.print("err unknown_verb ");
  io.print(verb);
  io.print('\n');
}

void VacuumDomain::get(Stream& io) {
  VacuumSystem& vacuum = app_.vacuum();
  io.print("ok kpa=");
  io.print(vacuum.currentKpa(), 2u);

  // A cleared target reads as unset rather than as 0.0, which is a real depth.
  io.print(" target=");
  const float target = vacuum.targetKpa();
  if (target <= 0.0f) {
    io.print('-');
  } else {
    io.print(target, 1u);
  }

  io.print(" floor=");
  io.print(cfg::CONTROL_FLOOR_KPA, 1u);
  io.print(" duty=");
  io.print(vacuum.pump().dutyPercent(), 1u);
  io.print(" state=");
  io.print(runStateName(vacuum.runState()));
  io.print(" sensor=");
  io.print(vacuum.sensor().present() ? "1" : "0");
  io.print(" fault=");
  const char* fault = vacuum.faultCode();
  io.print(fault[0] ? fault : "none");
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

  if (token == "-") {
    const MachineCommand clear{MachineCommand::Op::ClearTarget};
    xQueueSend(machineCommandQueue, &clear, 0);
    io.print("ok target=-\n");
    return;
  }

  float kpa = 0.0f;
  if (!parseFloat(token.c_str(), kpa)) {
    io.print("err bad_argument ");
    io.print(token);
    io.print('\n');
    return;
  }
  if (kpa < cfg::CONTROL_MIN_TARGET_KPA) {
    io.print("err out_of_range field=target value=");
    io.print(token);
    io.print(" min=");
    io.print(cfg::CONTROL_MIN_TARGET_KPA, 1u);
    io.print('\n');
    return;
  }

  const MachineCommand set{MachineCommand::Op::SetTarget, kpa};
  xQueueSend(machineCommandQueue, &set, 0);
  io.print("ok target=");
  io.print(kpa, 1u);
  io.print('\n');
}

void VacuumDomain::start(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  // The refusal is a pure read of the machine's target, fault and reading, so it is answered from
  // a dirty read; a would-succeed start is queued and the reply is optimistic.
  const char* refusal = app_.vacuum().startRefusal();
  if (refusal != nullptr) {
    io.print("err not_ready ");
    io.print(refusal);
    io.print('\n');
    return;
  }
  const MachineCommand start{MachineCommand::Op::Start};
  xQueueSend(machineCommandQueue, &start, 0);
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
  xQueueSend(machineCommandQueue, &stop, 0);
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
  xQueueSend(machineCommandQueue, &reset, 0);
  io.print("ok state=IDLE\n");
}

}  // namespace

Domain* createVacuumDomain(Machine& app) { return new VacuumDomain(app); }

}  // namespace gardener
