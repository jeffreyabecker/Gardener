#pragma once

#include <Arduino.h>

#include "routines/GestureQueue.h"
#include <Gfx.h>

namespace gardener {

// The panel's gestures behind the drawing side's interface: a scene takes the same presses
// `device.take_gesture()` would have answered, oldest first, and the pointer it is handed stays
// valid until the next take. The queue is the panel's own, so both readers drain one list and no
// press is delivered twice.
class PressQueue : public gfx::ui::GestureQueue {
 public:
  explicit PressQueue(gardener::GestureQueue& queue) : queue_(queue) {}

  const char* take() override {
    if (!queue_.take(press_)) return nullptr;
    return press_.c_str();
  }

 private:
  // Named in full: this class's base is the other queue of that name, and an unqualified one here
  // would find the base.
  gardener::GestureQueue& queue_;
  String press_;
};

}  // namespace gardener
