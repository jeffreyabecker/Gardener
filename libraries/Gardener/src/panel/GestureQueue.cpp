#include "panel/GestureQueue.h"

#include "core/Logger.h"

namespace gardener {

void GestureQueue::post(const char* gesture) {
  if (gesture == nullptr || gesture[0] == '\0') return;

  if (count_ == Capacity) {
    if (!overflowed_) {
      overflowed_ = true;
      if (logger_ != nullptr) logger_->warn("vm: button queue full, dropping the oldest");
    }
    head_ = (uint8_t)((head_ + 1) % Capacity);
    count_--;
  }

  const uint8_t tail = (uint8_t)((head_ + count_) % Capacity);
  presses_[tail] = Gestures::InternTable::instance().intern(gesture);
  count_++;
}

const char* GestureQueue::take() {
  if (count_ == 0) return nullptr;
  const char* gesture = presses_[head_];
  presses_[head_] = nullptr;
  head_ = (uint8_t)((head_ + 1) % Capacity);
  count_--;
  return gesture;
}

void GestureQueue::clear() {
  for (const char*& press : presses_)
    press = nullptr;
  head_ = 0;
  count_ = 0;
  overflowed_ = false;
}

}  // namespace gardener