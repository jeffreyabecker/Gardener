#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "panel/page/GestureQueue.h"

namespace gardener {

class Logger;

// The gestures the panel task has accepted and the page has not taken yet. Its writer is the panel
// task; its readers are the device binding's pull and the scene stack, so the two are never the same
// caller. A full queue drops the oldest, so the newest gesture still arrives, and the log says so
// once rather than on every drop.
//
// It is the drawing side's queue as well, so a page and the device binding drain one list and no
// press is delivered twice.
class GestureQueue : public gfx::ui::GestureQueue {
 public:
  static constexpr uint8_t Capacity = 8;

  void begin(Logger& logger) { logger_ = &logger; }

  void push(const String& gesture);
  bool take(String& gesture);
  void clear();

  // The oldest gesture a page has not taken, or nullptr when there is none. The pointer is this
  // queue's own and stays valid until the next take.
  const char* take() override;

 private:
  Logger* logger_ = nullptr;
  String presses_[Capacity];
  // What the last take() handed a page, which is what the pointer it answered points at.
  String press_;
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  bool overflowed_ = false;
};

}  // namespace gardener
