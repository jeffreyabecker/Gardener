#include "panel/GestureQueue.h"

#include "core/Gestures.h"

namespace gardener {

bool GestureQueue::post(const char* gesture) {
  if (gesture == nullptr || gesture[0] == '\0') return false;
  presses_.push(Gestures::InternTable::instance().intern(gesture));
  return true;
}

const char* GestureQueue::take() {
  if (presses_.empty()) return nullptr;
  const char* gesture = presses_.front();
  presses_.pop();
  return gesture;
}

void GestureQueue::clear() { presses_ = {}; }

}  // namespace gardener
