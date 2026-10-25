#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace gardener {

class Logger;

// The gestures the panel task has accepted and the page has not taken yet. Its writer is the panel
// task and its reader is the device binding's pull, so the two are never the same caller and the
// queue keeps a class of its own. A full queue drops the oldest, so the newest gesture still
// arrives, and the log says so once rather than on every drop.
class GestureQueue {
 public:
  static constexpr uint8_t Capacity = 8;

  void begin(Logger& logger) { logger_ = &logger; }

  void push(const String& gesture);
  bool take(String& gesture);
  void clear();

 private:
  Logger* logger_ = nullptr;
  String presses_[Capacity];
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  bool overflowed_ = false;
};

}  // namespace gardener
