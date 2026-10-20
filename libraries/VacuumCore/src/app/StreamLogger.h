#pragma once

#include <Arduino.h>
#include <string.h>

#include "app/Logger.h"

namespace gardener {

// The diagnostic sink for App's reports: three levels, framed as [I], [W] and [E] records on a
// stream. The stream is not its own — App hands it the same Serial the link writes replies to.
class StreamLogger : public Logger {
 public:
  explicit StreamLogger(Stream& stream) : stream_(stream) {}

  void info(const char* text) override { write('I', text); }
  void warn(const char* text) override { write('W', text); }
  void error(const char* text) override { write('E', text); }

 private:
  void write(char level, const char* text) {
    if (!text) return;
    stream_.write((uint8_t)'[');
    stream_.write((uint8_t)level);
    stream_.write((uint8_t)']');
    stream_.write((uint8_t)' ');
    stream_.write((const uint8_t*)text, strlen(text));
    stream_.write((uint8_t)'\n');
  }

  Stream& stream_;
};

}  // namespace gardener
