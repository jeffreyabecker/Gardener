#pragma once

#include <Arduino.h>
#include <string.h>

namespace gardener {

// The diagnostic sink for App's reports: three levels, framed as [I], [W] and [E] records on a
// stream. Neither the stream nor the on/off switch is its own — App hands it the same Serial the
// link writes replies to, and the one `device log` flag that silences every diagnostic.
class StreamLogger {
 public:
  StreamLogger(Stream& stream, const bool& enabled) : stream_(stream), enabled_(enabled) {}

  void info(const char* text) { write('I', text); }
  void warn(const char* text) { write('W', text); }
  void error(const char* text) { write('E', text); }

 private:
  void write(char level, const char* text) {
    if (!enabled_ || !text) return;
    stream_.write((uint8_t)'[');
    stream_.write((uint8_t)level);
    stream_.write((uint8_t)']');
    stream_.write((uint8_t)' ');
    stream_.write((const uint8_t*)text, strlen(text));
    stream_.write((uint8_t)'\n');
  }

  Stream& stream_;
  const bool& enabled_;
};

}  // namespace gardener
