#include "serial/PanelDomains.h"

#include <Arduino.h>
#include <stdint.h>

#include "Panel.h"
#include "BuzzerPlayer.h"
#include "serial/Domain.h"
#include "serial/Gate.h"

namespace gardener {
namespace {

String firstToken(const String& args) {
  const int space = args.indexOf(' ');
  return space < 0 ? args : args.substring(0, space);
}

class BuzzerDomain : public Domain {
 public:
  BuzzerDomain(Panel& app, Link& link) : Domain(link), app_(app) {}

  const char* name() const override { return "buzzer"; }
  void execute(Stream& io) override;

 private:
  void get(Stream& io, const String& args);
  void tone(Stream& io, const String& args);
  void play(Stream& io, const String& args);
  void stop(Stream& io, const String& args);

  Panel& app_;
};

void BuzzerDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > COMMAND_LINE_MAX) {
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
    if (requireControl(app_, link_, io)) tone(io, args);
  } else if (verb == "play") {
    if (requireControl(app_, link_, io)) play(io, args);
  } else if (verb == "stop") {
    if (requireControl(app_, link_, io)) stop(io, args);
  } else {
    io.print("err unknown_verb ");
    io.print(verb);
    io.print('\n');
  }
}

void BuzzerDomain::get(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  BuzzerPlayer& buzzer = app_.buzzer();
  io.print("ok tone=");
  if (!buzzer.sounding()) {
    io.print('-');
  } else {
    // The same compact `hz,ms` a tone reply has always used: the frequency now and what is left.
    io.print(buzzer.frequencyHz());
    io.print(',');
    io.print(buzzer.remainingMs(millis()));
  }
  io.print(" playing=");
  io.print(buzzer.playing() ? "on" : "off");
  io.print('\n');
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
    io.print("err bad_argument ");
    io.print(firstToken(extra));
    io.print('\n');
    return;
  }

  if (hzText.isEmpty() || msText.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }
  const float hz = hzText.toFloat();
  const int32_t ms = msText.toInt();
  if (ms < 0) {
    io.print("err bad_argument ");
    io.print(msText);
    io.print('\n');
    return;
  }

  if (hz < BuzzerMinHz || hz > BuzzerMaxHz) {
    io.print("err out_of_range field=tone value=");
    io.print(hzText);
    io.print(" min=");
    io.print(BuzzerMinHz, 1u);
    io.print(" max=");
    io.print(BuzzerMaxHz, 1u);
    io.print('\n');
    return;
  }

  app_.buzzer().tone((uint16_t)hz, ms);

  io.print("ok tone=");
  io.print((uint32_t)hz);
  io.print(',');
  io.print(ms);
  io.print('\n');
}

void BuzzerDomain::play(Stream& io, const String& args) {
  if (args.isEmpty()) {
    io.print("err bad_argument\n");
    return;
  }

  // The sequence is the rest of the line: no quotes, no escapes. It is checked here for the
  // reply, then sent raw — the control task parses it again inside play().
  const char* badToken = nullptr;
  if (!BuzzerPlayer::validate(args.c_str(), &badToken)) {
    io.print("err bad_argument");
    if (badToken && *badToken) {
      io.print(' ');
      io.print(badToken);
    }
    io.print('\n');
    return;
  }

  app_.buzzer().play(args.c_str());
  io.print("ok playing=on\n");
}

void BuzzerDomain::stop(Stream& io, const String& args) {
  if (!args.isEmpty()) {
    io.print("err bad_argument ");
    io.print(firstToken(args));
    io.print('\n');
    return;
  }

  app_.buzzer().off();
  io.print("ok playing=off\n");
}

}  // namespace

Domain* createBuzzerDomain(Panel& app, Link& link) { return new BuzzerDomain(app, link); }

}  // namespace gardener
