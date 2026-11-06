#include "RtosGestureQueue.h"

#include "core/Gestures.h"

namespace gardener::runtime {

RtosGestureQueue::RtosGestureQueue()
    : handle_(xQueueCreate(GestureQueueDepth, sizeof(const char*))) {}

bool RtosGestureQueue::post(const char* gesture) {
  if (handle_ == nullptr || gesture == nullptr || gesture[0] == '\0') return false;
  const char* word = Gestures::InternTable::instance().intern(gesture);
  return xQueueSend(handle_, &word, 0) == pdTRUE;
}

const char* RtosGestureQueue::take() {
  if (handle_ == nullptr) return nullptr;
  const char* word = nullptr;
  if (xQueueReceive(handle_, &word, 0) != pdTRUE) return nullptr;
  return word;
}

void RtosGestureQueue::clear() {
  if (handle_ != nullptr) xQueueReset(handle_);
}

}  // namespace gardener::runtime
