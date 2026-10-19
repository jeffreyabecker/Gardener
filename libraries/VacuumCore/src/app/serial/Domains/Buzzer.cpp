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

void readTone(const DomainContext& ctx, String& out) {
  BuzzerPlayer& buzzer = ctx.app.buzzer();
  if (!buzzer.sounding()) {
    addItem(out, "tone", nullptr);
    return;
  }
  // The same compact `hz,ms` a tone reply has always used: the frequency now and what is left.
  String value((uint32_t)buzzer.frequencyHz());
  value += ',';
  value += String(buzzer.remainingMs(millis()));
  addItem(out, "tone", value.c_str());
}

void readPlay(const DomainContext& ctx, String& out) {
  const String& sequence = ctx.app.buzzer().sequence();
  addItem(out, "play", sequence.length() > 0 ? sequence.c_str() : nullptr);
}

void readPlaying(const DomainContext& ctx, String& out) {
  addItemFlag(out, "playing", ctx.app.buzzer().playing());
}

const FieldSpec kFields[] = {
    {"tone", "text", "Hz", nullptr, &cfg::BUZZER_MIN_HZ, &cfg::BUZZER_MAX_HZ, readTone},
    {"play", "text", nullptr, nullptr, nullptr, nullptr, readPlay},
    {"playing", "bool", nullptr, nullptr, nullptr, nullptr, readPlaying},
};

constexpr uint8_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);

// A missing argument has no token to echo; anything the host did send is quoted back as it was.
const char* tokenOf(const String& args) { return args.isEmpty() ? nullptr : args.c_str(); }

String pageHeader(Stream& out, uint8_t rows, const char* topic = nullptr) {
  String header;
  if (topic) addItem(header, "topic", topic);
  addItem(header, "body", "rows");
  addItemU32(header, "rows", rows);
  replyOk(out, "help", "buzzer", header);
  return header;
}

class BuzzerDomain : public Domain {
 public:
  explicit BuzzerDomain(DomainContext& ctx) : ctx_(ctx) {}

  const char* name() const override { return "buzzer"; }

  void execute(Stream& input) override;
  void describe() override;
  void help(const char* topic) override;

 private:
  void get(const String& args);
  void tone(const String& args);
  void play(const String& args);
  void stop(const String& args);

  DomainContext& ctx_;
};

void BuzzerDomain::execute(Stream& input) {
  String line = input.readStringUntil('\n');
  line.trim();

  const int split = line.indexOf(' ');
  String verb = split < 0 ? line : line.substring(0, split);
  String args = split < 0 ? String() : line.substring(split + 1);
  args.trim();
  verb.toLowerCase();

  if (verb.isEmpty()) {
    replyBadArgument(ctx_.out, "buzzer", "-", nullptr);
  } else if (verb == "get") {
    get(args);
  } else if (verb == "tone") {
    tone(args);
  } else if (verb == "play") {
    play(args);
  } else if (verb == "stop") {
    stop(args);
  } else {
    replyUnknownVerb(ctx_.out, "buzzer", verb.c_str());
  }
}

void BuzzerDomain::get(const String& args) {
  if (args.isEmpty()) {
    replyBadArgument(ctx_.out, "buzzer", "get", nullptr);
    return;
  }

  String out;
  if (args.equalsIgnoreCase("all")) {
    readFields(kFields, kFieldCount, ctx_, out);
  } else if (!readField(kFields, kFieldCount, ctx_, args.c_str(), out)) {
    replyBadArgument(ctx_.out, "buzzer", "get", args.c_str());
    return;
  }
  replyOk(ctx_.out, "buzzer", "get", out);
}

void BuzzerDomain::tone(const String& args) {
  const int split = args.indexOf(' ');
  if (split < 0) {
    replyBadArgument(ctx_.out, "buzzer", "tone", tokenOf(args));
    return;
  }

  const String hzText = args.substring(0, split);
  String msText = args.substring(split + 1);
  msText.trim();

  float hz = 0.0f;
  uint32_t ms = 0;
  if (hzText.isEmpty() || msText.isEmpty() || !parseFloat(hzText.c_str(), hz) ||
      !parseU32(msText.c_str(), ms)) {
    replyBadArgument(ctx_.out, "buzzer", "tone", args.c_str());
    return;
  }

  if (hz < cfg::BUZZER_MIN_HZ || hz > cfg::BUZZER_MAX_HZ) {
    replyOutOfRange(ctx_.out, "buzzer", "tone", "tone", hzText.c_str(), &cfg::BUZZER_MIN_HZ,
                            &cfg::BUZZER_MAX_HZ);
    return;
  }

  ctx_.app.buzzer().tone((uint16_t)hz, ms);

  String out;
  readField(kFields, kFieldCount, ctx_, "tone", out);
  replyOk(ctx_.out, "buzzer", "tone", out);
}

void BuzzerDomain::play(const String& args) {
  if (args.isEmpty()) {
    replyBadArgument(ctx_.out, "buzzer", "play", nullptr);
    return;
  }

  String sequence;
  if (args.startsWith("\"")) {
    if (args.length() < 2 || !args.endsWith("\"")) {
      replyBadArgument(ctx_.out, "buzzer", "play", args.c_str());
      return;
    }
    const String quoted = args.substring(1, args.length() - 1);
    // One pass: `\"` and `\\` become the characters they stand for, and nothing is unescaped twice.
    for (unsigned int i = 0; i < quoted.length(); i++) {
      char character = quoted.charAt(i);
      if (character == '\\' && i + 1 < quoted.length() &&
          (quoted.charAt(i + 1) == '"' || quoted.charAt(i + 1) == '\\')) {
        character = quoted.charAt(++i);
      }
      sequence += character;
    }
  } else {
    // A sequence holds spaces, so an unquoted one is more than the single argument the verb takes.
    if (args.indexOf(' ') >= 0) {
      replyBadArgument(ctx_.out, "buzzer", "play", args.c_str());
      return;
    }
    sequence = args;
  }

  if (!ctx_.app.buzzer().play(sequence.c_str())) {
    replyBadArgument(ctx_.out, "buzzer", "play", args.c_str());
    return;
  }

  String out;
  readField(kFields, kFieldCount, ctx_, "play", out);
  replyOk(ctx_.out, "buzzer", "play", out);
}

void BuzzerDomain::stop(const String& args) {
  if (!args.isEmpty()) {
    replyBadArgument(ctx_.out, "buzzer", "stop", args.c_str());
    return;
  }

  ctx_.app.buzzer().off();

  String out;
  addItemFlag(out, "playing", false);
  replyOk(ctx_.out, "buzzer", "stop", out);
}

void BuzzerDomain::describe() {
  String row;
  addItem(row, "domain", name());
  addItemU32(row, "topics", 4);
  addItem(row, "about", "the piezo");
  replyRow(ctx_.out, row);
}

void BuzzerDomain::help(const char* topic) {
  if (!topic) {
    const String header = pageHeader(ctx_.out, 4);

    String getRow;
    addItem(getRow, "topic", "get");
    addItem(getRow, "args", "all|<field>");
    addItem(getRow, "about", "read every field or one");
    replyRow(ctx_.out, getRow);

    String toneRow;
    addItem(toneRow, "topic", "tone");
    addItem(toneRow, "args", "<hz> <ms>");
    addItem(toneRow, "about", "start a tone");
    replyRow(ctx_.out, toneRow);

    String playRow;
    addItem(playRow, "topic", "play");
    addItem(playRow, "args", "<sequence>");
    addItem(playRow, "about", "play a note sequence");
    replyRow(ctx_.out, playRow);

    String stopRow;
    addItem(stopRow, "topic", "stop");
    addItem(stopRow, "args", "");
    addItem(stopRow, "about", "silence the buzzer");
    replyRow(ctx_.out, stopRow);

    replyEnd(ctx_.out, "help", name(), header);
    return;
  }

  String key(topic);
  key.toLowerCase();

  if (key == "get") {
    const String header = pageHeader(ctx_.out, kFieldCount, "get");
    replyFieldHelp(ctx_.out, kFields, kFieldCount);
    replyEnd(ctx_.out, "help", name(), header);
    return;
  }

  if (key == "tone") {
    const String header = pageHeader(ctx_.out, 4, "tone");

    String hzRow;
    addItem(hzRow, "arg", "hz");
    addItem(hzRow, "type", "float");
    addItem(hzRow, "unit", "Hz");
    addItemFloat(hzRow, "min", cfg::BUZZER_MIN_HZ, 1);
    addItemFloat(hzRow, "max", cfg::BUZZER_MAX_HZ, 1);
    replyRow(ctx_.out, hzRow);

    String durationRow;
    addItem(durationRow, "arg", "ms");
    addItem(durationRow, "type", "int");
    addItem(durationRow, "unit", "ms");
    replyRow(ctx_.out, durationRow);

    String replyItem;
    addItem(replyItem, "reply", "tone");
    replyRow(ctx_.out, replyItem);

    String errorRow;
    addItem(errorRow, "error", "out_of_range");
    replyRow(ctx_.out, errorRow);

    replyEnd(ctx_.out, "help", name(), header);
    return;
  }

  if (key == "play") {
    const String header = pageHeader(ctx_.out, 3, "play");

    String sequenceRow;
    addItem(sequenceRow, "arg", "sequence");
    addItem(sequenceRow, "type", "text");
    replyRow(ctx_.out, sequenceRow);

    String replyItem;
    addItem(replyItem, "reply", "play");
    replyRow(ctx_.out, replyItem);

    String errorRow;
    addItem(errorRow, "error", "bad_argument");
    replyRow(ctx_.out, errorRow);

    replyEnd(ctx_.out, "help", name(), header);
    return;
  }

  if (key == "stop") {
    const String header = pageHeader(ctx_.out, 1, "stop");

    String replyItem;
    addItem(replyItem, "reply", "playing");
    replyRow(ctx_.out, replyItem);

    replyEnd(ctx_.out, "help", name(), header);
    return;
  }

  replyUnknownTopic(ctx_.out, "buzzer", topic);
}

}  // namespace

Domain* createBuzzerDomain(DomainContext& ctx) { return new BuzzerDomain(ctx); }

}  // namespace gardener




