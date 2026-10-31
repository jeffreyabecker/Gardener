#include "Script.h"

#include <math.h>
#include <stdlib.h>

#include <fstream>
#include <utility>

namespace gardener::walkthrough {
namespace {

// The device's beat: the unit a wait is rounded up into, because a beat is what the session
// advances.
constexpr int BeatMs = 250;

std::string trimmed(const std::string& text) {
  const char* spaces = " \t\r\n";
  const size_t first = text.find_first_not_of(spaces);
  if (first == std::string::npos) return std::string();
  const size_t last = text.find_last_not_of(spaces);
  return text.substr(first, last - first + 1);
}

// The first word of a line and what follows it, both trimmed.
void splitWord(const std::string& text, std::string& word, std::string& rest) {
  const std::string all = trimmed(text);
  const size_t space = all.find_first_of(" \t");
  if (space == std::string::npos) {
    word = all;
    rest.clear();
    return;
  }
  word = all.substr(0, space);
  rest = trimmed(all.substr(space + 1));
}

// A whole number: the text from its first character to its last, so `12v` and `1.2.3` are refused
// rather than read as `12` and `1.2`.
bool number(const std::string& text, float& value) {
  if (text.empty()) return false;
  char* end = nullptr;
  const float parsed = strtof(text.c_str(), &end);
  if (end != text.c_str() + text.size() || !isfinite(parsed)) return false;
  value = parsed;
  return true;
}

bool parseSettings(const std::string& text, Command& command, std::string& error) {
  size_t start = 0;
  while (start <= text.size()) {
    const size_t end = text.find(';', start);
    const std::string pair =
        trimmed(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (!pair.empty()) {
      const size_t equals = pair.find('=');
      if (equals == std::string::npos || equals == 0) {
        error = "a setting is key=value, not '" + pair + "'";
        return false;
      }
      command.settings.emplace_back(trimmed(pair.substr(0, equals)),
                                    trimmed(pair.substr(equals + 1)));
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return true;
}

bool parseLoad(const std::string& rest, Command& command, std::string& error) {
  std::string settings;
  splitWord(rest, command.scene, settings);
  if (command.scene.empty()) {
    error = "load takes a scene";
    return false;
  }
  return parseSettings(settings, command, error);
}

bool parseSend(const std::string& rest, Command& command, std::string& error) {
  std::string extra;
  splitWord(rest, command.gesture, extra);
  if (command.gesture.empty() || !extra.empty()) {
    error = "send takes one gesture";
    return false;
  }
  return true;
}

// The units are the file's, and the beats are the session's: a wait always advances the device at
// least as far as the line asked, and never more than one beat past it.
bool parseWait(const std::string& rest, Command& command, std::string& error) {
  const std::string text = trimmed(rest);
  char* end = nullptr;
  const double amount = text.empty() ? 0.0 : strtod(text.c_str(), &end);
  if (text.empty() || end == text.c_str() || !isfinite(amount) || amount < 0.0) {
    error = "wait takes a number of seconds or milliseconds";
    return false;
  }

  const std::string unit = trimmed(std::string(end));
  double ms = 0.0;
  if (unit == "s") {
    ms = amount * 1000.0;
  } else if (unit == "ms") {
    ms = amount;
  } else {
    error = "wait takes s or ms, not '" + unit + "'";
    return false;
  }

  const int beats = (int)ceil(ms / (double)BeatMs);
  command.beats = beats < 0 ? 0 : beats;
  return true;
}

bool parseSetRate(const std::string& rest, Command& command, std::string& error) {
  std::string first;
  std::string after;
  splitWord(rest, first, after);
  std::string second;
  std::string extra;
  splitWord(after, second, extra);

  float pull = 0.0f;
  float vent = 0.0f;
  if (!number(first, pull) || !number(second, vent) || !extra.empty() || !(pull > 0.0f) ||
      !(vent > 0.0f)) {
    error = "set-rate takes a pull and a vent rate in kPa per second, both above zero";
    return false;
  }
  command.pullKpaPerSecond = pull;
  command.ventKpaPerSecond = vent;
  return true;
}

bool parseLine(const std::string& text, int line, Command& command, std::string& error) {
  command = Command();
  command.line = line;

  std::string verb;
  std::string rest;
  splitWord(text, verb, rest);

  if (verb == "load") {
    command.verb = Command::Verb::Load;
    return parseLoad(rest, command, error);
  }
  if (verb == "send") {
    command.verb = Command::Verb::Send;
    return parseSend(rest, command, error);
  }
  if (verb == "wait") {
    command.verb = Command::Verb::Wait;
    return parseWait(rest, command, error);
  }
  if (verb == "set-rate") {
    command.verb = Command::Verb::SetRate;
    return parseSetRate(rest, command, error);
  }
  if (verb == "emit") {
    command.verb = Command::Verb::Emit;
    if (!rest.empty()) {
      error = "emit takes nothing";
      return false;
    }
    return true;
  }

  error = "no command '" + verb + "'";
  return false;
}

}  // namespace

bool readScript(const std::string& path, std::vector<Command>& commands, std::string& error) {
  std::ifstream file(path);
  if (!file) {
    error = path + " cannot be read";
    return false;
  }

  commands.clear();
  bool emitted = false;
  std::string text;
  int line = 0;
  while (std::getline(file, text)) {
    line++;
    const std::string stated = trimmed(text);
    if (stated.empty() || stated[0] == '#') continue;
    if (emitted) {
      error = "line " + std::to_string(line) + ": emit ends the file";
      return false;
    }

    Command command;
    std::string why;
    if (!parseLine(stated, line, command, why)) {
      error = "line " + std::to_string(line) + ": " + why;
      return false;
    }
    emitted = command.verb == Command::Verb::Emit;
    commands.push_back(std::move(command));
  }

  if (!emitted) {
    error = "the file names no emit";
    return false;
  }
  return true;
}

}  // namespace gardener::walkthrough
