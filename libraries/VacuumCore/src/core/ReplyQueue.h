#pragma once

#include <Arduino.h>

// Bounded hand-off queue between a producer and a consumer that must not know about
// each other: hosts write replies into it and the transport drains it, and the BLE
// callback parks inbound lines in one so all policy runs on the main loop.
class ReplyQueue {
 public:
  static constexpr uint8_t kCapacity = 8;

  bool push(const String& text) {
    if (count_ >= kCapacity) return false;
    items_[(head_ + count_) % kCapacity] = text;
    count_++;
    return true;
  }

  bool pop(String& out) {
    if (count_ == 0) return false;
    out = items_[head_];
    items_[head_] = "";
    head_ = (uint8_t)((head_ + 1) % kCapacity);
    count_--;
    return true;
  }

  bool empty() const { return count_ == 0; }
  uint8_t count() const { return count_; }

  void clear() {
    while (count_ > 0) {
      items_[head_] = "";
      head_ = (uint8_t)((head_ + 1) % kCapacity);
      count_--;
    }
  }

 private:
  String items_[kCapacity];
  uint8_t head_ = 0;
  uint8_t count_ = 0;
};
