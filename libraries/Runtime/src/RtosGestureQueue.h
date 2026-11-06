#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "panel/GestureQueue.h"

namespace gardener::runtime {

// The panel's queue on the device: one FreeRTOS queue of interned words, created when the
// composition builds it so it exists before either task that touches it runs. Its depth is its own,
// set where the queue is made rather than shared with the host's, which has none. A post never
// waits, so a full queue refuses the gesture rather than holding up the task that pressed it or
// throwing away a word that some other task put there.
class RtosGestureQueue : public GestureQueue {
 public:
  RtosGestureQueue();

  bool post(const char* gesture) override;
  const char* take() override;
  void clear() override;

 private:
  QueueHandle_t handle_ = nullptr;
};

}  // namespace gardener::runtime
