#include "serial/Domains.h"

#include <Arduino.h>

#include "Machine.h"
#include "Tasks.h"
#include "serial/Domain.h"
#include "serial/Gate.h"
#include "core/Config.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

// The machine's half of `device`: what the firmware is and which assemblies are fitted. It answers
// the fields a build without a panel still has — the panel's own domain answers these and the
// battery, log and unit fields beside them. The two are never registered on the same link, because
// a link holds one domain per name.
class MachineDeviceDomain : public Domain {
 public:
  MachineDeviceDomain(Machine& app, Link& link) : Domain(link), app_(app) {}

  const char* name() const override { return "device"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io);
  void reboot(Stream& io, const String& args);

  Machine& app_;
};

void MachineDeviceDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > cfg::COMMAND_LINE_MAX) {
    io.print("err bad_argument\n");
    return;
  }

  const int split = line.indexOf(' ');
  const String verb = split < 0 ? line : line.substring(0, split);
  String args;
  if (split >= 0) {
    args = line.substring(split + 1);
    args.trim();
  }

  if (verb.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }
  if (verb.equalsIgnoreCase("get")) {
    if (!args.isEmpty()) {
      io.print("err bad_argument ");
      io.print(firstToken(args));
      io.print('\n');
      return;
    }
    get(io);
    return;
  }
  if (verb.equalsIgnoreCase("reboot")) {
    if (!requireControl(app_, link_, io)) return;
    reboot(io, args);
    return;
  }

  io.print("err unknown_verb ");
  io.print(verb);
  io.print('\n');
}

// Every field is the machine's own: a build with no panel has no gauge to read, no log switch to
// persist and no unit to print.
void MachineDeviceDomain::get(Stream& io) {
  io.print("ok proto=");
  io.print(cfg::PROTOCOL_VERSION);
  io.print(" fw=");
  io.print(cfg::FIRMWARE_VERSION);
  io.print(" chip=");
  io.print(ESP.getChipModel());
  io.print(" heap=");
  io.print((uint32_t)ESP.getFreeHeap());
  // One byte, the same mask the BLE `caps` characteristic reports.
  io.print(" caps=");
  io.print(app_.capabilities().mask());
  io.print('\n');
}

void MachineDeviceDomain::reboot(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  // The reply has to leave the port before the reset takes it down, so the restart is a queued
  // command the control task applies on its next tick — the link never blocks on the vent wait.
  io.print("ok\n");
  const MachineCommand reboot{MachineCommand::Op::Reboot};
  xQueueSend(machineCommandQueue, &reboot, 0);
}

}  // namespace

Domain* createMachineDeviceDomain(Machine& app, Link& link) {
  return new MachineDeviceDomain(app, link);
}

}  // namespace gardener
