#include "RtosCommandQueue.h"

namespace gardener {

RtosCommandQueue::RtosCommandQueue()
    : handle_(xQueueCreate(CommandQueueDepth, sizeof(MachineCommand))) {}

bool RtosCommandQueue::post(const MachineCommand& command) {
  if (handle_ == nullptr) return false;
  return xQueueSend(handle_, &command, 0) == pdTRUE;
}

bool RtosCommandQueue::take(MachineCommand& command) {
  if (handle_ == nullptr) return false;
  return xQueueReceive(handle_, &command, 0) == pdTRUE;
}

}  // namespace gardener
