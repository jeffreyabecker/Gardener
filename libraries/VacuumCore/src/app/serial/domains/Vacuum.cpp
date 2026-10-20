#include <Arduino.h>
#include <stdint.h>

#include "app/AppContext.h"
#include "app/VacuumSystem.h"
#include "app/serial/Domain.h"
#include "app/serial/Domains.h"
#include "app/serial/Gate.h"
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
    io.print("err bad_argument\n");
    return;
  }

  const int space = line.indexOf(' ');
  String verb = space < 0 ? line : line.substring(0, (unsigned int)space);
  String args = space < 0 ? String() : line.substring((unsigned int)space + 1);
  args.trim();
  verb.toLowerCase();

  if (verb.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }
  if (verb == "get") {
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
  if (verb == "target") {
    if (!requireControl(app_, io)) return;
    target(io, args);
    return;
  }
  if (verb == "start") {
    if (!requireControl(app_, io)) return;
    start(io, args);
    return;
  }
  if (verb == "stop") {
    if (!requireControl(app_, io)) return;
    stop(io, args);
    return;
  }
  if (verb == "reset") {
    if (!requireControl(app_, io)) return;
    reset(io, args);
    return;
  }

  String reply = "err unknown_verb ";
  reply += verb;
  reply += '\n';
  io.print(reply);
}

void VacuumDomain::get(Stream& io) {
  VacuumSystem& vacuum = app_.vacuum();
  String reply = "ok kpa=";
  reply += String(vacuum.currentKpa(), 2u);

  // A cleared target reads as unset rather than as 0.0, which is a real depth.
  reply += " target=";
  const float target = vacuum.targetKpa();
  if (target <= 0.0f) {
    reply += '-';
  } else {
    reply += String(target, 1u);
  }

  reply += " floor=";
  reply += String(cfg::CONTROL_FLOOR_KPA, 1u);
  reply += " duty=";
  reply += String(vacuum.pump().dutyPercent(), 1u);
  reply += " state=";
  reply += runStateName(vacuum.runState());
  reply += " sensor=";
  reply += vacuum.sensor().present() ? "1" : "0";
  reply += " fault=";
  const char* fault = vacuum.faultCode();
  reply += fault[0] ? fault : "none";
  reply += " up=";
  reply += String(app_.uptimeMs());
  reply += '\n';
  io.print(reply);
}

void VacuumDomain::target(Stream& io, const String& args) {
  if (args.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }

  const int space = args.indexOf(' ');
  const String token = space < 0 ? args : args.substring(0, (unsigned int)space);
  String extra = space < 0 ? String() : args.substring((unsigned int)space + 1);
  extra.trim();
  if (!extra.isEmpty()) {
    String reply = "err bad_argument ";
    reply += firstToken(extra);
    reply += '\n';
    io.print(reply);
    return;
  }

  VacuumSystem& vacuum = app_.vacuum();
  if (token == "-") {
    vacuum.clearTarget();
    io.print("ok target=-\n");
    return;
  }

  float kpa = 0.0f;
  if (!parseFloat(token.c_str(), kpa)) {
    String reply = "err bad_argument ";
    reply += token;
    reply += '\n';
    io.print(reply);
    return;
  }
  if (kpa < cfg::CONTROL_MIN_TARGET_KPA) {
    String reply = "err out_of_range field=target value=";
    reply += token;
    reply += " min=";
    reply += String(cfg::CONTROL_MIN_TARGET_KPA, 1u);
    reply += '\n';
    io.print(reply);
    return;
  }

  vacuum.setTarget(kpa);
  String reply = "ok target=";
  reply += String(vacuum.targetKpa(), 1u);
  reply += '\n';
  io.print(reply);
}

void VacuumDomain::start(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    String reply = "err bad_argument ";
    reply += firstToken(args);
    reply += '\n';
    io.print(reply);
    return;
  }

  VacuumSystem& vacuum = app_.vacuum();
  if (!vacuum.start()) {
    String reply = "err not_ready ";
    reply += vacuum.startRefusal();
    reply += '\n';
    io.print(reply);
    return;
  }
  String reply = "ok state=";
  reply += runStateName(vacuum.runState());
  reply += '\n';
  io.print(reply);
}

void VacuumDomain::stop(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    String reply = "err bad_argument ";
    reply += firstToken(args);
    reply += '\n';
    io.print(reply);
    return;
  }

  VacuumSystem& vacuum = app_.vacuum();
  vacuum.stop();
  String reply = "ok state=";
  reply += runStateName(vacuum.runState());
  reply += '\n';
  io.print(reply);
}

void VacuumDomain::reset(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    String reply = "err bad_argument ";
    reply += firstToken(args);
    reply += '\n';
    io.print(reply);
    return;
  }

  VacuumSystem& vacuum = app_.vacuum();
  vacuum.resetFault();
  String reply = "ok state=";
  reply += runStateName(vacuum.runState());
  reply += '\n';
  io.print(reply);
}

}  // namespace

Domain* createVacuumDomain(AppContext& app) { return new VacuumDomain(app); }

}  // namespace gardener
