#include <Arduino.h>
#include <stdint.h>

#include "app/AppContext.h"
#include "app/VacuumSystem.h"
#include "app/serial/Domain.h"
#include "app/serial/Domains.h"
#include "core/Config.h"
#include "core/Parse.h"

namespace gardener {
namespace {

void readKpa(const DomainContext& ctx, String& out) {
  addItemFloat(out, "kpa", ctx.app.vacuum().currentKpa(), 2);
}

// A cleared target reads as unset rather than as 0.0, which is a real depth.
void readTarget(const DomainContext& ctx, String& out) {
  const float target = ctx.app.vacuum().targetKpa();
  if (target <= 0.0f) {
    addItem(out, "target", nullptr);
    return;
  }
  addItemFloat(out, "target", target, 1);
}

void readFloor(const DomainContext& ctx, String& out) {
  (void)ctx;
  addItemFloat(out, "floor", cfg::CONTROL_FLOOR_KPA, 1);
}

void readDuty(const DomainContext& ctx, String& out) {
  addItemFloat(out, "duty", ctx.app.vacuum().pump().dutyPercent(), 1);
}

void readState(const DomainContext& ctx, String& out) {
  addItem(out, "state", runStateName(ctx.app.vacuum().runState()));
}

void readSensor(const DomainContext& ctx, String& out) {
  addItem(out, "sensor", ctx.app.vacuum().sensor().present() ? "1" : "0");
}

void readFault(const DomainContext& ctx, String& out) {
  const char* code = ctx.app.vacuum().faultCode();
  addItem(out, "fault", code[0] ? code : "none");
}

void readUp(const DomainContext& ctx, String& out) {
  addItemU32(out, "up", ctx.app.uptimeMs());
}

const FieldSpec kFields[] = {
    {"kpa", "float", "kPa", nullptr, nullptr, nullptr, readKpa},
    // Only the shallow bound is a rule; a deeper target is accepted and the run simply never
    // arrives, which is what `floor` reports.
    {"target", "float", "kPa", nullptr, &cfg::CONTROL_MIN_TARGET_KPA, nullptr, readTarget},
    {"floor", "float", "kPa", nullptr, nullptr, nullptr, readFloor},
    {"duty", "float", "%", nullptr, nullptr, nullptr, readDuty},
    {"state", "choice", nullptr, "IDLE,PULL,HOLD,VENT,FAULT", nullptr, nullptr, readState},
    {"sensor", "bool", nullptr, nullptr, nullptr, nullptr, readSensor},
    {"fault", "text", nullptr, nullptr, nullptr, nullptr, readFault},
    {"up", "int", "ms", nullptr, nullptr, nullptr, readUp},
};

constexpr uint8_t kFieldCount = (uint8_t)(sizeof(kFields) / sizeof(kFields[0]));

const TopicSpec kTopics[] = {
    {"get", "all|<field>", "read every field or one"},
    {"target", "<kPa|->", "set or clear the target"},
    {"start", "", "close the valve and pull"},
    {"stop", "", "stop the pump and vent"},
    {"reset", "", "clear a latched fault"},
};

constexpr uint8_t kTopicCount = (uint8_t)(sizeof(kTopics) / sizeof(kTopics[0]));

class VacuumDomain : public Domain {
 public:
  explicit VacuumDomain(DomainContext& ctx) : ctx_(ctx) {}

  const char* name() const override { return "vacuum"; }

  void execute(Stream& input) override {
    String line = input.readStringUntil('\n');
    line.trim();

    const int space = line.indexOf(' ');
    String verb = space < 0 ? line : line.substring(0, (unsigned int)space);
    String args = space < 0 ? String() : line.substring((unsigned int)space + 1);
    args.trim();
    verb.toLowerCase();

    if (verb.isEmpty()) {
      replyBadArgument(ctx_.out, "vacuum", "-", nullptr);
      return;
    }
    if (verb == "get") {
      handleGet(args);
      return;
    }
    if (verb == "target") {
      handleTarget(args);
      return;
    }
    if (verb == "start") {
      handleStart(args);
      return;
    }
    if (verb == "stop") {
      handleStop(args);
      return;
    }
    if (verb == "reset") {
      handleReset(args);
      return;
    }
    replyUnknownVerb(ctx_.out, "vacuum", verb.c_str());
  }

  void describe() override {
    String row;
    addItem(row, "domain", "vacuum");
    addItemU32(row, "topics", kTopicCount);
    addItem(row, "about", "pressure and the run");
    replyRow(ctx_.out, row);
  }

  void help(const char* topic) override {
    if (!topic) {
      writeTopics();
      return;
    }

    String name = topic;
    name.toLowerCase();
    if (name == "get") {
      String header;
      beginPage("get", kFieldCount, header);
      replyFieldHelp(ctx_.out, kFields, kFieldCount);
      endPage(header);
      return;
    }
    if (name == "target") {
      String header;
      beginPage("target", 3, header);
      String arg;
      addItem(arg, "arg", "value");
      addItem(arg, "type", "float");
      addItem(arg, "unit", "kPa");
      addItemFloat(arg, "min", cfg::CONTROL_MIN_TARGET_KPA, 1);
      replyRow(ctx_.out, arg);
      String reply;
      addItem(reply, "reply", "target");
      replyRow(ctx_.out, reply);
      String error;
      addItem(error, "error", "out_of_range");
      replyRow(ctx_.out, error);
      endPage(header);
      return;
    }
    if (name == "start") {
      String header;
      beginPage("start", 2, header);
      writeReplyRow("state");
      writeErrorRow("not_ready");
      endPage(header);
      return;
    }
    if (name == "stop") {
      String header;
      beginPage("stop", 1, header);
      writeReplyRow("state");
      endPage(header);
      return;
    }
    if (name == "reset") {
      String header;
      beginPage("reset", 1, header);
      writeReplyRow("state");
      endPage(header);
      return;
    }
    replyUnknownTopic(ctx_.out, "vacuum", topic);
  }

 private:
  void handleGet(const String& args) {
    if (args.isEmpty()) {
      replyBadArgument(ctx_.out, "vacuum", "get", nullptr);
      return;
    }

    String items;
    if (args.equalsIgnoreCase("all")) {
      readFields(kFields, kFieldCount, ctx_, items);
      replyOk(ctx_.out, "vacuum", "get", items);
      return;
    }
    if (!readField(kFields, kFieldCount, ctx_, args.c_str(), items)) {
      replyBadArgument(ctx_.out, "vacuum", "get", args.c_str());
      return;
    }
    replyOk(ctx_.out, "vacuum", "get", items);
  }

  void handleTarget(const String& args) {
    if (args.isEmpty()) {
      replyBadArgument(ctx_.out, "vacuum", "target", nullptr);
      return;
    }

    const int space = args.indexOf(' ');
    const String token = space < 0 ? args : args.substring(0, (unsigned int)space);
    String extra = space < 0 ? String() : args.substring((unsigned int)space + 1);
    extra.trim();
    if (!extra.isEmpty()) {
      replyBadArgument(ctx_.out, "vacuum", "target", args.c_str());
      return;
    }

    VacuumSystem& vacuum = ctx_.app.vacuum();
    if (token == "-") {
      vacuum.clearTarget();
      String items;
      addItem(items, "target", nullptr);
      replyOk(ctx_.out, "vacuum", "target", items);
      return;
    }

    float kpa = 0.0f;
    if (!parseFloat(token.c_str(), kpa)) {
      replyBadArgument(ctx_.out, "vacuum", "target", token.c_str());
      return;
    }
    if (kpa < cfg::CONTROL_MIN_TARGET_KPA) {
      replyOutOfRange(ctx_.out, "vacuum", "target", "target", token.c_str(),
                              &cfg::CONTROL_MIN_TARGET_KPA, nullptr);
      return;
    }

    vacuum.setTarget(kpa);
    String items;
    addItemFloat(items, "target", vacuum.targetKpa(), 1);
    replyOk(ctx_.out, "vacuum", "target", items);
  }

  void handleStart(const String& args) {
    if (!args.isEmpty()) {
      replyBadArgument(ctx_.out, "vacuum", "start", args.c_str());
      return;
    }

    VacuumSystem& vacuum = ctx_.app.vacuum();
    if (!vacuum.start()) {
      replyNotReady(ctx_.out, "vacuum", "start", vacuum.startRefusal());
      return;
    }
    String items;
    addItem(items, "state", runStateName(vacuum.runState()));
    replyOk(ctx_.out, "vacuum", "start", items);
  }

  void handleStop(const String& args) {
    if (!args.isEmpty()) {
      replyBadArgument(ctx_.out, "vacuum", "stop", args.c_str());
      return;
    }

    VacuumSystem& vacuum = ctx_.app.vacuum();
    vacuum.stop();
    String items;
    addItem(items, "state", runStateName(vacuum.runState()));
    replyOk(ctx_.out, "vacuum", "stop", items);
  }

  void handleReset(const String& args) {
    if (!args.isEmpty()) {
      replyBadArgument(ctx_.out, "vacuum", "reset", args.c_str());
      return;
    }

    VacuumSystem& vacuum = ctx_.app.vacuum();
    vacuum.resetFault();
    String items;
    addItem(items, "state", runStateName(vacuum.runState()));
    replyOk(ctx_.out, "vacuum", "reset", items);
  }

  void beginPage(const char* topic, uint16_t rows, String& header) {
    if (topic) addItem(header, "topic", topic);
    addItem(header, "body", "rows");
    addItemU32(header, "rows", (uint32_t)rows);
    replyOk(ctx_.out, "help", "vacuum", header);
  }

  void endPage(const String& header) { replyEnd(ctx_.out, "help", "vacuum", header); }

  void writeReplyRow(const char* reply) {
    String row;
    addItem(row, "reply", reply);
    replyRow(ctx_.out, row);
  }

  void writeErrorRow(const char* error) {
    String row;
    addItem(row, "error", error);
    replyRow(ctx_.out, row);
  }

  void writeTopics() {
    String header;
    beginPage(nullptr, kTopicCount, header);
    for (uint8_t i = 0; i < kTopicCount; i++) {
      String row;
      addItem(row, "topic", kTopics[i].name);
      addItem(row, "args", kTopics[i].args);
      addItem(row, "about", kTopics[i].about);
      replyRow(ctx_.out, row);
    }
    endPage(header);
  }

  DomainContext& ctx_;
};

}  // namespace

Domain* createVacuumDomain(DomainContext& ctx) { return new VacuumDomain(ctx); }

}  // namespace gardener


