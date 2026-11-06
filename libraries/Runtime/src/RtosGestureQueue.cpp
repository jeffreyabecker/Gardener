#include "RtosGestureQueue.h"

#include "core/Gestures.h"

namespace gardener::runtime {
namespace {

// What the queue holds. A beat drains it and a beat classifies at most one gesture, so the depth is
// not what a burst needs: it is what a drain that stopped leaves behind, set far past anything a
// running panel can bring.
constexpr uint8_t QueueDepth = 64;

}  // namespace

RtosGestureQueue::RtosGestureQueue()
    : handle_(xQueueCreate(QueueDepth, sizeof(const char*))) {}

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
