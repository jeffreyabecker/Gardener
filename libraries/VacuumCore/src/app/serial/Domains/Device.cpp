#include "app/serial/Domains.h"

#include <Arduino.h>

#include "app/AppContext.h"
#include "app/Settings.h"
#include "app/serial/Domain.h"
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
    replyBadArgument(io, nullptr);
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
    replyBadArgument(io, nullptr);
    return;
  }
  if (verb.equalsIgnoreCase("get")) {
    if (!args.isEmpty()) {
      replyBadArgument(io, firstToken(args).c_str());
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
  replyUnknownVerb(io, verb.c_str());
}

void DeviceDomain::get(Stream& io) {
  String out;
  addItemU32(out, "proto", cfg::PROTOCOL_VERSION);
  addItem(out, "fw", cfg::FIRMWARE_VERSION);
  addItem(out, "chip", ESP.getChipModel());
  addItemU32(out, "heap", (uint32_t)ESP.getFreeHeap());

  // Both battery fields report `-` together, and only the gauge knows whether there is one.
  if (app_.battery().present()) {
    addItemFloat(out, "batt_v", app_.battery().volts(), 2);
    addItemU32(out, "batt_pct", app_.battery().percent());
  } else {
    addItem(out, "batt_v", nullptr);
    addItem(out, "batt_pct", nullptr);
  }

  addItemFlag(out, "log", app_.logEnabled());
  // The token, not the suffix: a host reads `kpa`, and the panel prints `kPa`.
  addItem(out, "unit", displayUnitInfo(app_.settings().unit()).name);

  // `widthxheight` in pixels, one item because it is one fact about the panel.
  const String size = String(app_.display().width()) + "x" + String(app_.display().height());
  addItem(out, "display-size", size.c_str());

  replyOk(io, out);
}

void DeviceDomain::set(Stream& io, const String& args) {
  // `set` is `<field> <value>`, and every value this domain takes is one token.
  const int split = args.indexOf(' ');
  if (split < 0) {
    replyBadArgument(io, nullptr);
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
    replyBadArgument(io, nullptr);
    return;
  }

  if (field.equalsIgnoreCase("log")) {
    bool enabled = false;
    if (!parseFlag(value.c_str(), enabled)) {
      replyBadArgument(io, value.c_str());
      return;
    }
    // Refused before it is applied: a command is all or nothing.
    if (!extra.isEmpty()) {
      replyBadArgument(io, firstToken(extra).c_str());
      return;
    }
    app_.logEnabled() = enabled;

    String out;
    addItemFlag(out, "log", enabled);
    replyOk(io, out);
    return;
  }

  if (field.equalsIgnoreCase("unit")) {
    DisplayUnit unit = DisplayUnit::Kpa;
    if (!parseDisplayUnit(value.c_str(), unit)) {
      replyBadArgument(io, value.c_str());
      return;
    }
    if (!extra.isEmpty()) {
      replyBadArgument(io, firstToken(extra).c_str());
      return;
    }
    app_.settings().setUnit(unit);

    String out;
    addItem(out, "unit", displayUnitInfo(unit).name);
    replyOk(io, out);
    return;
  }

  replyBadArgument(io, field.c_str());
}

void DeviceDomain::reboot(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    replyBadArgument(io, firstToken(args).c_str());
    return;
  }

  // The reply has to leave the port before the reset takes it down.
  replyOk(io, "");
  delay(50);
  ESP.restart();
}

}  // namespace

Domain* createDeviceDomain(AppContext& app) { return new DeviceDomain(app); }

}  // namespace gardener
