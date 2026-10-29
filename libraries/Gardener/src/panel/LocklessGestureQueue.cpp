#include "Build.h"

#if GARDENER_PANEL

#include "panel/LocklessGestureQueue.h"

#include "core/Logger.h"

namespace gardener::panel {

void LocklessGestureQueue::push(const String& gesture) {
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

bool LocklessGestureQueue::take(String& gesture) {
  if (count_ == 0) return false;
  gesture = presses_[head_];
  presses_[head_] = "";
  head_ = (uint8_t)((head_ + 1) % Capacity);
  count_--;
  return true;
}

const char* LocklessGestureQueue::take() {
  if (!take(press_)) return nullptr;
  return press_.c_str();
}

void LocklessGestureQueue::clear() {
  for (String& press : presses_) press = "";
  head_ = 0;
  count_ = 0;
  overflowed_ = false;
}

}  // namespace gardener::panel
#endif
