#pragma once

#include <Arduino.h>
#include <string.h>

#include "Logger.h"

namespace gardener {

// The diagnostic sink: three levels, framed as [I], [W] and [E] records on the serial line the link
// writes its replies to. It writes every level from boot, and the links narrow the set; a record for
// a level that is not in it is not written, and the replies are unaffected either way.
class SerialLogger : public Logger {
 public:
  explicit SerialLogger(Stream& serial) : serial_(serial) { levels_.reserve(LevelsReserve); }

  String levels() const override { return levels_; }
  void setLevels(const String& levels) override { levels_ = levels; }

  void info(const char* text) override { write('I', text); }
  void warn(const char* text) override { write('W', text); }
  void error(const char* text) override { write('E', text); }

 private:
  // The longest set, `IWE`, and its terminator. The buffer is reserved so a change never
  // reallocates: a reader on another task may then see a torn value for one call, which drops or
  // repeats a single record and nothing more.
  static constexpr unsigned int LevelsReserve = 4;

  void write(char level, const char* text) {
    if (!text || levels_.indexOf(level) < 0) return;
    serial_.write((uint8_t)'[');
    serial_.write((uint8_t)level);
    serial_.write((uint8_t)']');
    serial_.write((uint8_t)' ');
    serial_.write((const uint8_t*)text, strlen(text));
    serial_.write((uint8_t)'\n');
  }

  Stream& serial_;
  // The levels written, every one from boot; written by a link on the interface task and read on the
  // control and panel beats.
  String levels_ = "IWE";
};

}  // namespace gardener
