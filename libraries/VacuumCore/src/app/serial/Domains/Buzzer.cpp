#include "app/serial/Domains.h"

#include <Arduino.h>
#include <stdint.h>

#include "app/AppContext.h"
#include "app/BuzzerPlayer.h"
#include "app/serial/Domain.h"
#include "core/Config.h"
#include "core/Parse.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

class BuzzerDomain : public Domain {
 public:
  explicit BuzzerDomain(AppContext& app) : app_(app) {}

  const char* name() const override { return "buzzer"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io, const String& args);
  void tone(Stream& io, const String& args);
  void play(Stream& io, const String& args);
  void stop(Stream& io, const String& args);

  AppContext& app_;
};

void BuzzerDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > cfg::COMMAND_LINE_MAX) {
    replyBadArgument(io, nullptr);
    return;
  }

  const int split = line.indexOf(' ');
  String verb = split < 0 ? line : line.substring(0, split);
  String args = split < 0 ? String() : line.substring(split + 1);
  args.trim();
  verb.toLowerCase();

  if (verb.isEmpty()) {
    replyBadArgument(io, nullptr);
  } else if (verb == "get") {
    get(io, args);
  } else if (verb == "tone") {
    tone(io, args);
  } else if (verb == "play") {
    play(io, args);
  } else if (verb == "stop") {
    stop(io, args);
  } else {
    replyUnknownVerb(io, verb.c_str());
  }
}

void BuzzerDomain::get(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    replyBadArgument(io, firstToken(args).c_str());
    return;
  }

  BuzzerPlayer& buzzer = app_.buzzer();
  String out;
  if (!buzzer.sounding()) {
    addItem(out, "tone", nullptr);
  } else {
    // The same compact `hz,ms` a tone reply has always used: the frequency now and what is left.
    String value((uint32_t)buzzer.frequencyHz());
    value += ',';
    value += String(buzzer.remainingMs(millis()));
    addItem(out, "tone", value.c_str());
  }
  addItemFlag(out, "playing", buzzer.playing());
  replyOk(io, out);
}

void BuzzerDomain::tone(Stream& io, const String& args) {
  const int split = args.indexOf(' ');
  if (split < 0) {
    replyBadArgument(io, nullptr);
    return;
  }

  const String hzText = args.substring(0, split);
  String rest = args.substring(split + 1);
  rest.trim();

  const int durationEnd = rest.indexOf(' ');
  const String msText = durationEnd < 0 ? rest : rest.substring(0, durationEnd);
  String extra;
  if (durationEnd >= 0) {
    extra = rest.substring(durationEnd + 1);
    extra.trim();
  }
  if (!extra.isEmpty()) {
    replyBadArgument(io, firstToken(extra).c_str());
    return;
  }

  float hz = 0.0f;
  uint32_t ms = 0;
  if (hzText.isEmpty() || msText.isEmpty()) {
    replyBadArgument(io, nullptr);
    return;
  }
  if (!parseFloat(hzText.c_str(), hz)) {
    replyBadArgument(io, hzText.c_str());
    return;
  }
  if (!parseU32(msText.c_str(), ms)) {
    replyBadArgument(io, msText.c_str());
    return;
  }

  if (hz < cfg::BUZZER_MIN_HZ || hz > cfg::BUZZER_MAX_HZ) {
    replyOutOfRange(io, "tone", hzText.c_str(), &cfg::BUZZER_MIN_HZ, &cfg::BUZZER_MAX_HZ);
    return;
  }

  app_.buzzer().tone((uint16_t)hz, ms);

  String out;
  String value((uint32_t)hz);
  value += ',';
  value += String(ms);
  addItem(out, "tone", value.c_str());
  replyOk(io, out);
}

void BuzzerDomain::play(Stream& io, const String& args) {
  if (args.isEmpty()) {
    replyBadArgument(io, nullptr);
    return;
  }

  // The sequence is the rest of the line: no quotes, no escapes.
  const char* badToken = nullptr;
  if (!app_.buzzer().play(args.c_str(), &badToken)) {
    replyBadArgument(io, badToken);
    return;
  }

  String out;
  addItemFlag(out, "playing", true);
  replyOk(io, out);
}

void BuzzerDomain::stop(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    replyBadArgument(io, firstToken(args).c_str());
    return;
  }

  app_.buzzer().off();

  String out;
  addItemFlag(out, "playing", false);
  replyOk(io, out);
}

}  // namespace

Domain* createBuzzerDomain(AppContext& app) { return new BuzzerDomain(app); }

}  // namespace gardener
