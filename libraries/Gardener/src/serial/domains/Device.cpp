#include "Build.h"

#include "serial/Domains.h"

#include <Arduino.h>

#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "firmwareversion.h"
#include "core/Logger.h"
#include "core/Battery.h"
#include "serial/Domain.h"
#include "serial/SerialLink.h"
#include "storage/Config.h"
#if GARDENER_PANEL
#include "panel/Panel.h"
#include "core/Units.h"
#include "serial/PanelDomains.h"
#include "storage/DeviceConfig.h"
#endif

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

// The `device` domain: what the firmware is and which assemblies are fitted. One class serves both
// builds, because the wire surface is one surface — a build with a panel answers the gauge and unit
// fields beside the machine's, and a machine-alone build answers the machine's alone. The two are
// never registered on the same link, because a link holds one domain per name.
class DeviceDomain : public Domain {
 public:
  // The panel's build passes its gauge and document; a machine-alone build has neither, and the
  // fields that would read them are not compiled.
  DeviceDomain(Machine& app, CommandOutbox& outbox, const BoardFacts& board,
               const Battery* battery = nullptr, Config* config = nullptr)
      : Domain(outbox), app_(app), board_(board), battery_(battery), config_(config) {}

  const char* name() const override { return "device"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io);
  void set(Stream& io, const String& args);
  void reboot(Stream& io, const String& args);

  Machine& app_;
  BoardFacts board_;
  const Battery* battery_;
  Config* config_;
};

void DeviceDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > COMMAND_LINE_MAX) {
    io.print("err bad_argument\n");
    return;
  }

  // The verb ends at the first space; everything after it belongs to the verb's own grammar. The
  // domains do not share a tokenizer.
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

void DeviceDomain::get(Stream& io) {
  io.print("ok proto=");
  io.print(PROTOCOL_VERSION);
  io.print(" fw=");
  io.print(FirmwareVersion);
  io.print(" chip=");
  io.print(board_.chip());
  io.print(" heap=");
  io.print(board_.heap());

#if GARDENER_PANEL
  // Both battery fields report `-` together, and only the gauge knows whether there is one.
  io.print(" batt_v=");
  if (battery_->present()) {
    io.print(battery_->volts(), 2u);
  } else {
    io.print('-');
  }
  io.print(" batt_pct=");
  if (battery_->present()) {
    io.print(battery_->percent());
  } else {
    io.print('-');
  }
#endif

  io.print(" log=");
  io.print(app_.logger().levels());

#if GARDENER_PANEL
  // The token, not the suffix: a host reads `kpa`, and the panel prints `kPa`.
  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(config_->get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  io.print(" unit=");
  io.print(displayUnitInfo(unit).name);
#endif

  // One byte, the same mask the BLE `caps` characteristic reports.
  io.print(" caps=");
  io.print(app_.capabilities());
  io.print('\n');
}

void DeviceDomain::set(Stream& io, const String& args) {
  // `set` is `<field> <value>`, and every value this domain takes is one token.
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

#if GARDENER_PANEL
  // The machine's settings are build symbols now, so `unit` is the only preference this domain
  // takes; a machine-alone build has no panel to print in one.
  if (field.equalsIgnoreCase("unit")) {
    DisplayUnit unit = DisplayUnit::Kpa;
    if (!parseDisplayUnit(value.c_str(), unit)) {
      io.print("err bad_argument ");
      io.print(value);
      io.print('\n');
      return;
    }
    if (!extra.isEmpty()) {
      io.print("err bad_argument ");
      io.print(firstToken(extra));
      io.print('\n');
      return;
    }
    config_->set(DeviceSection, UnitKey, displayUnitInfo(unit).name);
    config_->save();

    io.print("ok unit=");
    io.print(displayUnitInfo(unit).name);
    io.print('\n');
    return;
  }
#endif

  io.print("err bad_argument ");
  io.print(field);
  io.print('\n');
}

void DeviceDomain::reboot(Stream& io, const String& args) {
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

// The machine-alone build's `device`: the machine's own fields. A build with a panel registers the
// panel's instead, which answers these and the gauge and unit beside them.
Domain* createMachineDeviceDomain(Machine& app, CommandOutbox& outbox, const BoardFacts& board) {
  return new DeviceDomain(app, outbox, board);
}

#if GARDENER_PANEL
Domain* createDeviceDomain(panel::Panel& app, CommandOutbox& outbox, const BoardFacts& board) {
  return new DeviceDomain(app, outbox, board, &app.battery(), &app.config());
}
#endif

}  // namespace gardener
