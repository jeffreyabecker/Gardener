#include <Arduino.h>

#include "core/Battery.h"
#include "core/Logger.h"
#include "core/Units.h"
#include "core/firmwareversion.h"
#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "serial/SerialLink.h"
#include "storage/DeviceSettings.h"

namespace gardener {
inline namespace serial {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

// What the firmware is and which assemblies are fitted. The gauge and the device's document are the
// composition's, so they come through the parts; the machine's own fields come from the machine.
void get(Stream& io, Machine& machine, const SerialParts& parts) {
  io.print("ok proto=");
  io.print(PROTOCOL_VERSION);
  io.print(" fw=");
  io.print(FirmwareVersion);
  io.print(" chip=");
  io.print(parts.board.chip());
  io.print(" heap=");
  io.print(parts.board.heap());

  // Both battery fields report `-` together, and only the gauge knows whether there is one.
  io.print(" batt_v=");
  if (parts.battery.present()) {
    io.print(parts.battery.volts(), 2u);
  } else {
    io.print('-');
  }
  io.print(" batt_pct=");
  if (parts.battery.present()) {
    io.print(parts.battery.percent());
  } else {
    io.print('-');
  }

  io.print(" log=");
  io.print(machine.logger().levels());

  // The token, not the suffix: a host reads `kpa`, and the panel prints `kPa`.
  io.print(" unit=");
  io.print(displayUnitInfo(parts.settings.unit()).name);

  // One byte, the same mask the BLE `caps` characteristic reports.
  io.print(" caps=");
  io.print(machine.capabilities());
  io.print('\n');
}

void set(Stream& io, const String& args, Machine& machine, DeviceSettings& settings) {
  // `set` is `<field> <value>`, and every value this grammar takes is one token.
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
    machine.logger().setLevels(levels);

    io.print("ok log=");
    io.print(levels);
    io.print('\n');
    return;
  }

  // The machine's settings are build symbols, so `unit` is the only preference this grammar takes.
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
    settings.setUnit(unit);
    settings.save();

    io.print("ok unit=");
    io.print(displayUnitInfo(unit).name);
    io.print('\n');
    return;
  }

  io.print("err bad_argument ");
  io.print(field);
  io.print('\n');
}

void reboot(Stream& io, const String& args, CommandOutbox& outbox) {
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
  outbox.post(reboot);
}

}  // namespace

// The device's own fields, the level set it writes through the logger, the unit it keeps and the
// restart it queues.
void SerialLink::executeDevice(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > COMMAND_LINE_MAX) {
    io.print("err bad_argument\n");
    return;
  }

  // The verb ends at the first space; everything after it belongs to the verb's own grammar. The
  // grammars do not share a tokenizer.
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
    get(io, parts_.machine, parts_);
    return;
  }
  if (verb.equalsIgnoreCase("set")) {
    set(io, args, parts_.machine, parts_.settings);
    return;
  }
  if (verb.equalsIgnoreCase("reboot")) {
    reboot(io, args, outbox_);
    return;
  }

  io.print("err unknown_verb ");
  io.print(verb);
  io.print('\n');
}

}  // namespace serial
}  // namespace gardener
