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

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

class VacuumDomain : public Domain {
 public:
  explicit VacuumDomain(AppContext& app) : app_(app) {}

  const char* name() const override { return "vacuum"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io);
  void target(Stream& io, const String& args);
  void start(Stream& io, const String& args);
  void stop(Stream& io, const String& args);
  void reset(Stream& io, const String& args);

  AppContext& app_;
};

void VacuumDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > cfg::COMMAND_LINE_MAX) {
    replyBadArgument(io, nullptr);
    return;
  }

  const int space = line.indexOf(' ');
  String verb = space < 0 ? line : line.substring(0, (unsigned int)space);
  String args = space < 0 ? String() : line.substring((unsigned int)space + 1);
  args.trim();
  verb.toLowerCase();

  if (verb.isEmpty()) {
    replyBadArgument(io, nullptr);
    return;
  }
  if (verb == "get") {
    if (!args.isEmpty()) {
      replyBadArgument(io, firstToken(args).c_str());
      return;
    }
    get(io);
    return;
  }
  if (verb == "target") {
    target(io, args);
    return;
  }
  if (verb == "start") {
    start(io, args);
    return;
  }
  if (verb == "stop") {
    stop(io, args);
    return;
  }
  if (verb == "reset") {
    reset(io, args);
    return;
  }
  replyUnknownVerb(io, verb.c_str());
}

void VacuumDomain::get(Stream& io) {
  VacuumSystem& vacuum = app_.vacuum();
  String out;
  addItemFloat(out, "kpa", vacuum.currentKpa(), 2);

  // A cleared target reads as unset rather than as 0.0, which is a real depth.
  const float target = vacuum.targetKpa();
  if (target <= 0.0f) {
    addItem(out, "target", nullptr);
  } else {
    addItemFloat(out, "target", target, 1);
  }

  addItemFloat(out, "floor", cfg::CONTROL_FLOOR_KPA, 1);
  addItemFloat(out, "duty", vacuum.pump().dutyPercent(), 1);
  addItem(out, "state", runStateName(vacuum.runState()));
  addItem(out, "sensor", vacuum.sensor().present() ? "1" : "0");
  const char* fault = vacuum.faultCode();
  addItem(out, "fault", fault[0] ? fault : "none");
  addItemU32(out, "up", app_.uptimeMs());
  replyOk(io, out);
}

void VacuumDomain::target(Stream& io, const String& args) {
  if (args.isEmpty()) {
    replyBadArgument(io, nullptr);
    return;
  }

  const int space = args.indexOf(' ');
  const String token = space < 0 ? args : args.substring(0, (unsigned int)space);
  String extra = space < 0 ? String() : args.substring((unsigned int)space + 1);
  extra.trim();
  if (!extra.isEmpty()) {
    replyBadArgument(io, firstToken(extra).c_str());
    return;
  }

  VacuumSystem& vacuum = app_.vacuum();
  if (token == "-") {
    vacuum.clearTarget();
    String items;
    addItem(items, "target", nullptr);
    replyOk(io, items);
    return;
  }

  float kpa = 0.0f;
  if (!parseFloat(token.c_str(), kpa)) {
    replyBadArgument(io, token.c_str());
    return;
  }
  if (kpa < cfg::CONTROL_MIN_TARGET_KPA) {
    replyOutOfRange(io, "target", token.c_str(), &cfg::CONTROL_MIN_TARGET_KPA, nullptr);
    return;
  }

  vacuum.setTarget(kpa);
  String items;
  addItemFloat(items, "target", vacuum.targetKpa(), 1);
  replyOk(io, items);
}

void VacuumDomain::start(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    replyBadArgument(io, firstToken(args).c_str());
    return;
  }

  VacuumSystem& vacuum = app_.vacuum();
  if (!vacuum.start()) {
    replyNotReady(io, vacuum.startRefusal());
    return;
  }
  String items;
  addItem(items, "state", runStateName(vacuum.runState()));
  replyOk(io, items);
}

void VacuumDomain::stop(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    replyBadArgument(io, firstToken(args).c_str());
    return;
  }

  VacuumSystem& vacuum = app_.vacuum();
  vacuum.stop();
  String items;
  addItem(items, "state", runStateName(vacuum.runState()));
  replyOk(io, items);
}

void VacuumDomain::reset(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    replyBadArgument(io, firstToken(args).c_str());
    return;
  }

  VacuumSystem& vacuum = app_.vacuum();
  vacuum.resetFault();
  String items;
  addItem(items, "state", runStateName(vacuum.runState()));
  replyOk(io, items);
}

}  // namespace

Domain* createVacuumDomain(AppContext& app) { return new VacuumDomain(app); }

}  // namespace gardener
