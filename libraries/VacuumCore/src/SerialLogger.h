#pragma once

#include <Arduino.h>
#include <string.h>

#include "Logger.h"

namespace gardener {

// The diagnostic sink: three levels, framed as [I], [W] and [E] records on the serial line the link
// writes its replies to. It starts enabled, and the links switch it; while it is off it writes
// nothing and the replies are unaffected.
class SerialLogger : public Logger {
 public:
  explicit SerialLogger(Stream& serial) : serial_(serial) {}

  void setEnabled(bool enabled) override { enabled_ = enabled; }
  bool enabled() const override { return enabled_; }

  void info(const char* text) override { write('I', text); }
  void warn(const char* text) override { write('W', text); }
  void error(const char* text) override { write('E', text); }

 private:
  void write(char level, const char* text) {
    if (!enabled_ || !text) return;
    serial_.write((uint8_t)'[');
    serial_.write((uint8_t)level);
    serial_.write((uint8_t)']');
    serial_.write((uint8_t)' ');
    serial_.write((const uint8_t*)text, strlen(text));
    serial_.write((uint8_t)'\n');
  }

  Stream& serial_;
  // Written by a link on the interface task, read on the control and panel beats. A byte write is
  // atomic on this part, and volatile keeps the compiler from caching the flag in a task.
  volatile bool enabled_ = true;
};

}  // namespace gardener
