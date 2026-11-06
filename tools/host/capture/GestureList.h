#pragma once

#include <stdint.h>

#include "core/Gestures.h"
#include "panel/GestureQueue.h"

namespace gardener {

// The gesture queue the host tools use, where the panel's is the device's: a ring of the words the
// panel was handed. It behaves as the device's does — a full ring refuses the word rather than
// throwing an older one away — and differs only in what it does not need: no task ever posts to it
// from another thread, so it takes no lock.
class GestureList : public GestureQueue {
 public:
  bool post(const char* gesture) override {
    if (gesture == nullptr || gesture[0] == '\0') return false;
    if (count_ == GestureQueueDepth) return false;
    const uint8_t tail = (uint8_t)((head_ + count_) % GestureQueueDepth);
    presses_[tail] = Gestures::InternTable::instance().intern(gesture);
    count_++;
    return true;
  }

  const char* take() override {
    if (count_ == 0) return nullptr;
    const char* gesture = presses_[head_];
    presses_[head_] = nullptr;
    head_ = (uint8_t)((head_ + 1) % GestureQueueDepth);
    count_--;
    return gesture;
  }

  void clear() override {
    for (const char*& press : presses_)
      press = nullptr;
    head_ = 0;
    count_ = 0;
  }

 private:
  const char* presses_[GestureQueueDepth] = {};
  uint8_t head_ = 0;
  uint8_t count_ = 0;
};

}  // namespace gardener
