#pragma once

#include <Arduino.h>
#include <string.h>

#include "core/Logger.h"

namespace gardener {

// The diagnostic sink: two levels, framed as [I] and [W] records on the serial line the link writes
// its replies to.
class SerialLogger : public Logger {
 public:
  explicit SerialLogger(Stream& serial) : serial_(serial) {}

  using Logger::log;
  void log(LogLevel level, const char* text) override { write(logLevelLetter(level), text); }

 private:
  void write(char level, const char* text) {
    if (!text) return;
    serial_.write((uint8_t)'[');
    serial_.write((uint8_t)level);
    serial_.write((uint8_t)']');
    serial_.write((uint8_t)' ');
    serial_.write((const uint8_t*)text, strlen(text));
    serial_.write((uint8_t)'\n');
  }

  Stream& serial_;
};

}  // namespace gardener