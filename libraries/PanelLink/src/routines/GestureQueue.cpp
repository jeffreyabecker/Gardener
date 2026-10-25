#include "routines/GestureQueue.h"

#include "Logger.h"

namespace gardener {

void GestureQueue::push(const String& gesture) {
  if (gesture.isEmpty()) return;

  if (count_ == Capacity) {
    if (!overflowed_) {
      overflowed_ = true;
      logger_->warn("vm: button queue full, dropping the oldest");
    }
    head_ = (uint8_t)((head_ + 1) % Capacity);
    count_--;
  }

  const uint8_t tail = (uint8_t)((head_ + count_) % Capacity);
  presses_[tail] = gesture;
  count_++;
}

bool GestureQueue::take(String& gesture) {
  if (count_ == 0) return false;
  gesture = presses_[head_];
  presses_[head_] = "";
  head_ = (uint8_t)((head_ + 1) % Capacity);
  count_--;
  return true;
}

void GestureQueue::clear() {
  for (String& press : presses_) press = "";
  head_ = 0;
  count_ = 0;
  overflowed_ = false;
}

}  // namespace gardener
