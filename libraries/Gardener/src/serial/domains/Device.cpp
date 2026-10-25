#include "Build.h"

#if GARDENER_PANEL

#include "serial/PanelDomains.h"

#include <Arduino.h>

#include "Panel.h"
#include "MachineCommand.h"
#include "firmwareversion.h"
#include "Logger.h"
#include "serial/SerialLink.h"
#include "hal/Battery.h"
#include "hal/Display.h"
#include "panel/Units.h"
#include "serial/Domain.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

class DeviceDomain : public Domain {
 public:
  DeviceDomain(Panel& app, CommandOutbox& outbox) : Domain(outbox), app_(app) {}

  const char* name() const override { return "device"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io);
  void set(Stream& io, const String& args);
  void reboot(Stream& io, const String& args);

  Panel& app_;
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
  io.print(ESP.getChipModel());
  io.print(" heap=");
  io.print((uint32_t)ESP.getFreeHeap());

  // Both battery fields report `-` together, and only the gauge knows whether there is one.
  io.print(" batt_v=");
  if (app_.battery().present()) {
    io.print(app_.battery().volts(), 2u);
  } else {
    io.print('-');
  }
  io.print(" batt_pct=");
  if (app_.battery().present()) {
    io.print(app_.battery().percent());
  } else {
    io.print('-');
  }

  io.print(" log=");
  io.print(app_.logger().levels());
  // The token, not the suffix: a host reads `kpa`, and the panel prints `kPa`.
  io.print(" unit=");
  io.print(displayUnitInfo(app_.unit()).name);
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

  // The machine's settings are build symbols now, so `unit` is the only preference this domain
  // takes.
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
    app_.setUnit(unit);

    io.print("ok unit=");
    io.print(displayUnitInfo(unit).name);
    io.print('\n');
    return;
  }

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

Domain* createDeviceDomain(Panel& app, CommandOutbox& outbox) {
  return new DeviceDomain(app, outbox);
}

}  // namespace gardener
#endif
