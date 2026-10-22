#include <freertos/queue.h>

#include <cstring>
#include <deque>
#include <vector>

// The command queue the machine's native modules post to, for the host: the same items, in the same
// order, with the wait ignored because a check never blocks. `machineCommandQueue` itself is defined
// with the rest of the host's stand-ins.

struct HostQueue {
  std::deque<std::vector<uint8_t>> items;
  size_t itemSize = 0;
  size_t depth = 0;
};

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize) {
  auto* queue = new HostQueue();
  queue->itemSize = itemSize;
  queue->depth = length;
  return queue;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t) {
  if (queue == nullptr || item == nullptr) return pdFALSE;
  if (queue->items.size() >= queue->depth) return pdFALSE;
  const auto* bytes = static_cast<const uint8_t*>(item);
  queue->items.emplace_back(bytes, bytes + queue->itemSize);
  return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void* out, TickType_t) {
  if (queue == nullptr || out == nullptr || queue->items.empty()) return pdFALSE;
  std::vector<uint8_t>& item = queue->items.front();
  memcpy(out, item.data(), queue->itemSize);
  queue->items.pop_front();
  return pdTRUE;
}

void vTaskDelay(TickType_t) {}
