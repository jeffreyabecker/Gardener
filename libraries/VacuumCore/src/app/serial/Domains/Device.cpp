#include "app/serial/Domains.h"

#include <Arduino.h>
#include <strings.h>

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

// --- the field readers -----------------------------------------------------
//
// One per field, and the only place a field's value is formatted: `get all`, a single-field read
// and the help page all reach a field through the table below, so none of them can disagree about
// which fields exist or what one means.

void readProto(const DomainContext&, String& out) { addItemU32(out, "proto", cfg::PROTOCOL_VERSION); }

void readFirmware(const DomainContext&, String& out) { addItem(out, "fw", cfg::FIRMWARE_VERSION); }

void readChip(const DomainContext&, String& out) { addItem(out, "chip", ESP.getChipModel()); }

void readHeap(const DomainContext&, String& out) {
  addItemU32(out, "heap", (uint32_t)ESP.getFreeHeap());
}

void readBatteryVolts(const DomainContext& ctx, String& out) {
  // Both battery fields report `-` together, and only the gauge knows whether there is one.
  if (!ctx.app.battery().present()) {
    addItem(out, "batt_v", nullptr);
    return;
  }
  addItemFloat(out, "batt_v", ctx.app.battery().volts(), 2);
}

void readBatteryPercent(const DomainContext& ctx, String& out) {
  if (!ctx.app.battery().present()) {
    addItem(out, "batt_pct", nullptr);
    return;
  }
  addItemU32(out, "batt_pct", ctx.app.battery().percent());
}

void readLog(const DomainContext& ctx, String& out) { addItemFlag(out, "log", ctx.logEnabled); }

// The token, not the suffix: a host reads `kpa`, and the panel prints `kPa`.
void readUnit(const DomainContext& ctx, String& out) {
  addItem(out, "unit", displayUnitInfo(ctx.app.settings().unit()).name);
}

// `widthxheight` in pixels, one item because it is one fact about the panel.
void readDisplaySize(const DomainContext& ctx, String& out) {
  const String size =
      String(ctx.app.display().width()) + "x" + String(ctx.app.display().height());
  addItem(out, "display-size", size.c_str());
}

const FieldSpec kFields[] = {
    {"proto", "int", nullptr, nullptr, nullptr, nullptr, readProto},
    {"fw", "text", nullptr, nullptr, nullptr, nullptr, readFirmware},
    {"chip", "text", nullptr, nullptr, nullptr, nullptr, readChip},
    {"heap", "int", "bytes", nullptr, nullptr, nullptr, readHeap},
    {"batt_v", "float", "V", nullptr, nullptr, nullptr, readBatteryVolts},
    {"batt_pct", "int", "%", nullptr, nullptr, nullptr, readBatteryPercent},
    {"log", "bool", nullptr, "on,off", nullptr, nullptr, readLog},
    {"unit", "choice", nullptr, "kpa,psi,inhg,mmhg", nullptr, nullptr, readUnit},
    {"display-size", "text", nullptr, nullptr, nullptr, nullptr, readDisplaySize},
};

constexpr uint8_t kFieldCount = (uint8_t)(sizeof(kFields) / sizeof(kFields[0]));

const TopicSpec kTopics[] = {
    {"get", "all|<field>", "read every field or one"},
    {"set", "log <on|off>|unit <unit>", "write a device setting"},
    {"reboot", "", "restart the device"},
};

constexpr uint8_t kTopicCount = (uint8_t)(sizeof(kTopics) / sizeof(kTopics[0]));

// A topic page's header and its `end` line carry the same items, so a host can match the two
// without remembering anything it was told.
String pageItems(const char* topic, uint8_t rows) {
  String items;
  addItem(items, "topic", topic);
  addItem(items, "body", "rows");
  addItemU32(items, "rows", rows);
  return items;
}

class DeviceDomain : public Domain {
 public:
  explicit DeviceDomain(DomainContext& context) : ctx_(context) {}

  const char* name() const override { return "device"; }
  void execute(Stream& input) override;
  void describe() override;
  void help(const char* topic) override;

 private:
  void executeGet(const String& args);
  void executeSet(const String& args);
  void executeReboot(const String& args);

  DomainContext& ctx_;
};

void DeviceDomain::execute(Stream& input) {
  String line = input.readStringUntil('\n');
  line.trim();

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
    replyBadArgument(ctx_.out, "device", "-", nullptr);
    return;
  }
  if (verb.equalsIgnoreCase("get")) {
    executeGet(args);
    return;
  }
  if (verb.equalsIgnoreCase("set")) {
    executeSet(args);
    return;
  }
  if (verb.equalsIgnoreCase("reboot")) {
    executeReboot(args);
    return;
  }
  replyUnknownVerb(ctx_.out, "device", verb.c_str());
}

void DeviceDomain::executeGet(const String& args) {
  if (args.isEmpty()) {
    replyBadArgument(ctx_.out, "device", "get", nullptr);
    return;
  }

  String out;
  if (args.equalsIgnoreCase("all")) {
    readFields(kFields, kFieldCount, ctx_, out);
    replyOk(ctx_.out, "device", "get", out);
    return;
  }

  if (!readField(kFields, kFieldCount, ctx_, args.c_str(), out)) {
    replyBadArgument(ctx_.out, "device", "get", args.c_str());
    return;
  }
  replyOk(ctx_.out, "device", "get", out);
}

void DeviceDomain::executeSet(const String& args) {
  // `set` is `<field> <value>`, and every value this domain takes is one token.
  const int split = args.indexOf(' ');
  if (split < 0) {
    replyBadArgument(ctx_.out, "device", "set", nullptr);
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
    replyBadArgument(ctx_.out, "device", "set", nullptr);
    return;
  }

  if (field.equalsIgnoreCase("log")) {
    bool enabled = false;
    if (!parseFlag(value.c_str(), enabled)) {
      replyBadArgument(ctx_.out, "device", "set", value.c_str());
      return;
    }
    // Refused before it is applied: a command is all or nothing.
    if (!extra.isEmpty()) {
      replyBadArgument(ctx_.out, "device", "set", extra.c_str());
      return;
    }
    ctx_.logEnabled = enabled;

    String out;
    addItemFlag(out, "log", enabled);
    replyOk(ctx_.out, "device", "set", out);
    return;
  }

  if (field.equalsIgnoreCase("unit")) {
    DisplayUnit unit = DisplayUnit::Kpa;
    if (!parseDisplayUnit(value.c_str(), unit)) {
      replyBadArgument(ctx_.out, "device", "set", value.c_str());
      return;
    }
    if (!extra.isEmpty()) {
      replyBadArgument(ctx_.out, "device", "set", extra.c_str());
      return;
    }
    ctx_.app.settings().setUnit(unit);

    String out;
    addItem(out, "unit", displayUnitInfo(unit).name);
    replyOk(ctx_.out, "device", "set", out);
    return;
  }

  replyBadArgument(ctx_.out, "device", "set", field.c_str());
}

void DeviceDomain::executeReboot(const String& args) {
  if (!args.isEmpty()) {
    replyBadArgument(ctx_.out, "device", "reboot", args.c_str());
    return;
  }

  // The reply has to leave the port before the reset takes it down.
  replyOk(ctx_.out, "device", "reboot", "");
  delay(50);
  ESP.restart();
}

void DeviceDomain::describe() {
  String row;
  addItem(row, "domain", name());
  addItemU32(row, "topics", kTopicCount);
  addItem(row, "about", "the machine and this stream");
  replyRow(ctx_.out, row);
}

void DeviceDomain::help(const char* topic) {
  if (!topic) {
    String header;
    addItem(header, "body", "rows");
    addItemU32(header, "rows", kTopicCount);
    replyOk(ctx_.out, "help", "device", header);

    for (uint8_t i = 0; i < kTopicCount; i++) {
      String row;
      addItem(row, "topic", kTopics[i].name);
      addItem(row, "args", kTopics[i].args);
      addItem(row, "about", kTopics[i].about);
      replyRow(ctx_.out, row);
    }

    replyEnd(ctx_.out, "help", "device", header);
    return;
  }

  if (strcasecmp(topic, "get") == 0) {
    const String header = pageItems("get", kFieldCount);
    replyOk(ctx_.out, "help", "device", header);
    replyFieldHelp(ctx_.out, kFields, kFieldCount);
    replyEnd(ctx_.out, "help", "device", header);
    return;
  }

  if (strcasecmp(topic, "set") == 0) {
    const String header = pageItems("set", 5);
    replyOk(ctx_.out, "help", "device", header);

    String fieldArg;
    addItem(fieldArg, "arg", "field");
    addItem(fieldArg, "type", "choice");
    addItem(fieldArg, "choices", "log,unit");
    replyRow(ctx_.out, fieldArg);

    String valueArg;
    addItem(valueArg, "arg", "value");
    addItem(valueArg, "type", "text");
    replyRow(ctx_.out, valueArg);

    String logField;
    addItem(logField, "field", "log");
    addItem(logField, "type", "bool");
    addItem(logField, "access", "rw");
    addItem(logField, "choices", "on,off");
    replyRow(ctx_.out, logField);

    String unitField;
    addItem(unitField, "field", "unit");
    addItem(unitField, "type", "choice");
    addItem(unitField, "access", "rw");
    addItem(unitField, "choices", "kpa,psi,inhg,mmhg");
    replyRow(ctx_.out, unitField);

    String reply;
    addItem(reply, "reply", "log|unit");
    replyRow(ctx_.out, reply);

    replyEnd(ctx_.out, "help", "device", header);
    return;
  }

  if (strcasecmp(topic, "reboot") == 0) {
    const String header = pageItems("reboot", 1);
    replyOk(ctx_.out, "help", "device", header);
    String row;
    addItem(row, "reply", "reboot");
    replyRow(ctx_.out, row);
    replyEnd(ctx_.out, "help", "device", header);
    return;
  }

  replyUnknownTopic(ctx_.out, "device", topic);
}

}  // namespace

Domain* createDeviceDomain(DomainContext& context) { return new DeviceDomain(context); }

}  // namespace gardener


