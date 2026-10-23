#include "serial/Domains.h"

#include <Arduino.h>

#include "Machine.h"
#include "MachineCommand.h"
#include "firmwareversion.h"
#include "Logger.h"
#include "serial/Domain.h"
#include "serial/MachineFields.h"
#include "serial/SerialLink.h"
#include "storage/MachineSettings.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

// The machine's half of `device`: what the firmware is and which assemblies are fitted. It answers
// the fields a build without a panel still has — the panel's own domain answers these and the
// battery and unit fields beside them. The two are never registered on the same link, because
// a link holds one domain per name.
class MachineDeviceDomain : public Domain {
 public:
  MachineDeviceDomain(Machine& app, CommandOutbox& outbox) : Domain(outbox), app_(app) {}

  const char* name() const override { return "device"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io);
  void set(Stream& io, const String& args);
  void reboot(Stream& io, const String& args);

  Machine& app_;
};

void MachineDeviceDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > COMMAND_LINE_MAX) {
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
  if (verb.equalsIgnoreCase("set")) {
    set(io, args);
    return;
  }
  if (verb.equalsIgnoreCase("reboot")) {
    reboot(io, args);
    return;
  }

  io.print("err unknown_verb ");
  io.print(verb);
  io.print('\n');
}

// Every field is the machine's own: a build with no panel has no gauge to read and no unit to
// print. The diagnostics switch is the logger's, not a preference, so this build serves it too.
void MachineDeviceDomain::get(Stream& io) {
  io.print("ok proto=");
  io.print(PROTOCOL_VERSION);
  io.print(" fw=");
  io.print(FirmwareVersion);
  io.print(" chip=");
  io.print(ESP.getChipModel());
  io.print(" heap=");
  io.print((uint32_t)ESP.getFreeHeap());
  io.print(" log=");
  io.print(app_.logger().levels());
  io.print(" flow=");
  io.print(app_.settings().pumpFlowLpm(), 2u);
  io.print(" volume=");
  io.print(app_.settings().chamberVolumeL(), 3u);
  io.print(" floor=");
  io.print(app_.settings().floorPressureKpa(), 1u);
  io.print(" pump_max=");
  io.print(app_.settings().pumpMaxPercent(), 1u);
  io.print(" deadband=");
  io.print(app_.settings().deadbandPressureKpa(), 2u);
  // The fitted direction, not the stored key: a pending change takes effect on the next boot.
  io.print(" valve=");
  io.print(app_.vacuum().vent().energiseWhenOpen() ? "closed" : "open");
  // One byte, the same mask the BLE `caps` characteristic reports.
  io.print(" caps=");
  io.print(app_.capabilities());
  io.print('\n');
}

void MachineDeviceDomain::set(Stream& io, const String& args) {
  // `set` is `<field> <value>`, and `log`, `flow`, `volume`, `floor`, `pump_max`, `deadband` and
  // `valve` are the fields this build has. Everything but `log` is applied on the control task:
  // the value is checked and refused here, then posted, so one task owns the machine state.
  const int split = args.indexOf(' ');
  if (split < 0) {
    io.print("err bad_argument\n");
    return;
  }

  const String field = args.substring(0, split);
  String rest = args.substring(split + 1);
  rest.trim();

  const int valueEnd = rest.indexOf(' ');
  const String value = valueEnd < 0 ? rest : rest.substring(0, valueEnd);
  String extra;
  if (valueEnd >= 0) {
    extra = rest.substring(valueEnd + 1);
    extra.trim();
  }

  if (value.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }

  if (field.equalsIgnoreCase("log")) {
    String levels;
    if (!parseLevels(value, levels)) {
      io.print("err bad_argument ");
      io.print(value);
      io.print('\n');
      return;
    }
    // Refused before it is applied: a command is all or nothing.
    if (!extra.isEmpty()) {
      io.print("err bad_argument ");
      io.print(firstToken(extra));
      io.print('\n');
      return;
    }
    app_.logger().setLevels(levels);

    io.print("ok log=");
    io.print(levels);
    io.print('\n');
    return;
  }

  // The six machine settings are shared with the panel build's domain, so the bounds, the refusals
  // and the task that applies them are one definition rather than two.
  if (setMachineField(io, app_, outbox_, field, value, extra)) return;

  io.print("err bad_argument ");
  io.print(field);
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
  outbox_.post(reboot);
}

}  // namespace

Domain* createMachineDeviceDomain(Machine& app, CommandOutbox& outbox) {
  return new MachineDeviceDomain(app, outbox);
}

}  // namespace gardener
