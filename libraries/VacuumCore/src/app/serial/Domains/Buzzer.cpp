#include "app/serial/Domains.h"

#include <Arduino.h>
#include <stdint.h>

#include "app/AppContext.h"
#include "app/BuzzerPlayer.h"
#include "app/serial/Domain.h"
#include "app/serial/Gate.h"
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
    io.print("err bad_argument\n");
    return;
  }

  const int split = line.indexOf(' ');
  String verb = split < 0 ? line : line.substring(0, split);
  String args = split < 0 ? String() : line.substring(split + 1);
  args.trim();
  verb.toLowerCase();

  if (verb.isEmpty()) {
    io.print("err bad_argument\n");
  } else if (verb == "get") {
    get(io, args);
  } else if (verb == "tone") {
    if (requireControl(app_, io)) tone(io, args);
  } else if (verb == "play") {
    if (requireControl(app_, io)) play(io, args);
  } else if (verb == "stop") {
    if (requireControl(app_, io)) stop(io, args);
  } else {
    String reply = "err unknown_verb ";
    reply += verb;
    reply += '\n';
    io.print(reply);
  }
}

void BuzzerDomain::get(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    String reply = "err bad_argument ";
    reply += firstToken(args);
    reply += '\n';
    io.print(reply);
    return;
  }

  BuzzerPlayer& buzzer = app_.buzzer();
  String reply = "ok tone=";
  if (!buzzer.sounding()) {
    reply += '-';
  } else {
    // The same compact `hz,ms` a tone reply has always used: the frequency now and what is left.
    reply += String((uint32_t)buzzer.frequencyHz());
    reply += ',';
    reply += String(buzzer.remainingMs(millis()));
  }
  reply += " playing=";
  reply += buzzer.playing() ? "on" : "off";
  reply += '\n';
  io.print(reply);
}

void BuzzerDomain::tone(Stream& io, const String& args) {
  const int split = args.indexOf(' ');
  if (split < 0) {
    io.print("err bad_argument\n");
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
    String reply = "err bad_argument ";
    reply += firstToken(extra);
    reply += '\n';
    io.print(reply);
    return;
  }

  float hz = 0.0f;
  uint32_t ms = 0;
  if (hzText.isEmpty() || msText.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }
  if (!parseFloat(hzText.c_str(), hz)) {
    String reply = "err bad_argument ";
    reply += hzText;
    reply += '\n';
    io.print(reply);
    return;
  }
  if (!parseU32(msText.c_str(), ms)) {
    String reply = "err bad_argument ";
    reply += msText;
    reply += '\n';
    io.print(reply);
    return;
  }

  if (hz < cfg::BUZZER_MIN_HZ || hz > cfg::BUZZER_MAX_HZ) {
    String reply = "err out_of_range field=tone value=";
    reply += hzText;
    reply += " min=";
    reply += String(cfg::BUZZER_MIN_HZ, 1u);
    reply += " max=";
    reply += String(cfg::BUZZER_MAX_HZ, 1u);
    reply += '\n';
    io.print(reply);
    return;
  }

  app_.buzzer().tone((uint16_t)hz, ms);

  String reply = "ok tone=";
  reply += String((uint32_t)hz);
  reply += ',';
  reply += String(ms);
  reply += '\n';
  io.print(reply);
}

void BuzzerDomain::play(Stream& io, const String& args) {
  if (args.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }

  // The sequence is the rest of the line: no quotes, no escapes.
  const char* badToken = nullptr;
  if (!app_.buzzer().play(args.c_str(), &badToken)) {
    String reply = "err bad_argument";
    if (badToken && *badToken) {
      reply += ' ';
      reply += badToken;
    }
    reply += '\n';
    io.print(reply);
    return;
  }

  io.print("ok playing=on\n");
}

void BuzzerDomain::stop(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    String reply = "err bad_argument ";
    reply += firstToken(args);
    reply += '\n';
    io.print(reply);
    return;
  }

  app_.buzzer().off();
  io.print("ok playing=off\n");
}

}  // namespace

Domain* createBuzzerDomain(AppContext& app) { return new BuzzerDomain(app); }

}  // namespace gardener
