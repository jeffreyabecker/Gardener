#include "app/serial/Domains.h"

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/SafeReboot.h"
#include "app/serial/Domain.h"
#include "app/serial/Gate.h"
#include "app/storage/Settings.h"
#include "core/Config.h"
#include "core/Parse.h"
#include "core/Units.h"
#include "hal/Battery.h"
#include "hal/Display.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

class DeviceDomain : public Domain {
 public:
  explicit DeviceDomain(AppContext& app) : app_(app) {}

  const char* name() const override { return "device"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io);
  void set(Stream& io, const String& args);
  void reboot(Stream& io, const String& args);

  AppContext& app_;
};

void DeviceDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > cfg::COMMAND_LINE_MAX) {
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
      String reply = "err bad_argument ";
      reply += firstToken(args);
      reply += '\n';
      io.print(reply);
      return;
    }
    get(io);
    return;
  }
  if (verb.equalsIgnoreCase("set")) {
    if (!requireControl(app_, io)) return;
    set(io, args);
    return;
  }
  if (verb.equalsIgnoreCase("reboot")) {
    if (!requireControl(app_, io)) return;
    reboot(io, args);
    return;
  }

  String reply = "err unknown_verb ";
  reply += verb;
  reply += '\n';
  io.print(reply);
}

void DeviceDomain::get(Stream& io) {
  String reply = "ok proto=";
  reply += String(cfg::PROTOCOL_VERSION);
  reply += " fw=";
  reply += cfg::FIRMWARE_VERSION;
  reply += " chip=";
  reply += ESP.getChipModel();
  reply += " heap=";
  reply += String((uint32_t)ESP.getFreeHeap());

  // Both battery fields report `-` together, and only the gauge knows whether there is one.
  reply += " batt_v=";
  if (app_.battery().present()) {
    reply += String(app_.battery().volts(), 2u);
  } else {
    reply += '-';
  }
  reply += " batt_pct=";
  if (app_.battery().present()) {
    reply += String(app_.battery().percent());
  } else {
    reply += '-';
  }

  reply += " log=";
  reply += app_.logEnabled() ? "on" : "off";
  // The token, not the suffix: a host reads `kpa`, and the panel prints `kPa`.
  reply += " unit=";
  reply += displayUnitInfo(app_.settings().unit()).name;
  // `widthxheight` in pixels, one field because it is one fact about the panel.
  reply += " display-size=";
  reply += String(app_.display().width());
  reply += 'x';
  reply += String(app_.display().height());
  reply += '\n';
  io.print(reply);
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
    bool enabled = false;
    if (!parseFlag(value.c_str(), enabled)) {
      String reply = "err bad_argument ";
      reply += value;
      reply += '\n';
      io.print(reply);
      return;
    }
    // Refused before it is applied: a command is all or nothing.
    if (!extra.isEmpty()) {
      String reply = "err bad_argument ";
      reply += firstToken(extra);
      reply += '\n';
      io.print(reply);
      return;
    }
    app_.logEnabled() = enabled;

    String reply = "ok log=";
    reply += enabled ? "on" : "off";
    reply += '\n';
    io.print(reply);
    return;
  }

  if (field.equalsIgnoreCase("unit")) {
    DisplayUnit unit = DisplayUnit::Kpa;
    if (!parseDisplayUnit(value.c_str(), unit)) {
      String reply = "err bad_argument ";
      reply += value;
      reply += '\n';
      io.print(reply);
      return;
    }
    if (!extra.isEmpty()) {
      String reply = "err bad_argument ";
      reply += firstToken(extra);
      reply += '\n';
      io.print(reply);
      return;
    }
    app_.settings().setUnit(unit);

    String reply = "ok unit=";
    reply += displayUnitInfo(unit).name;
    reply += '\n';
    io.print(reply);
    return;
  }

  String reply = "err bad_argument ";
  reply += field;
  reply += '\n';
  io.print(reply);
}

void DeviceDomain::reboot(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    String reply = "err bad_argument ";
    reply += firstToken(args);
    reply += '\n';
    io.print(reply);
    return;
  }

  // The reply has to leave the port before the reset takes it down. The safe path then releases the
  // chamber before restarting, so a controller cannot skip that on its way out.
  io.print("ok\n");
  delay(50);
  safeReboot(app_);
}

}  // namespace

Domain* createDeviceDomain(AppContext& app) { return new DeviceDomain(app); }

}  // namespace gardener
