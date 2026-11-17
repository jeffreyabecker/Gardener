#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "machine/CommandQueue.h"

namespace gardener::runtime {

// The device's queue: one FreeRTOS queue, created when the composition builds it so it exists
// before either task runs. A post never blocks, so a full queue refuses the command rather than
// holding up the task that posted it.
class RtosCommandQueue : public CommandQueue {
 public:
  RtosCommandQueue();

  bool post(const MachineCommand& command) override;
  bool take(MachineCommand& command) override;

 private:
  QueueHandle_t handle_ = nullptr;
};

}  // namespace gardener::runtime