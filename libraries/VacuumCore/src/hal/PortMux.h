#pragma once

#include <Arduino.h>

namespace gardener {

// FreeRTOS critical section for a short handoff between an ISR or another task and the loop.
//
// enter() masks interrupts on this core and takes the object's spinlock, so the handoff it
// protects is a few instructions and cannot block or inherit a priority. It is not an ISR
// entry and must not wrap anything longer than a pointer swap.
class PortMux {
 public:
  PortMux() = default;
  PortMux(const PortMux&) = delete;
  PortMux& operator=(const PortMux&) = delete;

  void enter() { portENTER_CRITICAL(&mux_); }
  void exit() { portEXIT_CRITICAL(&mux_); }

 private:
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

}  // namespace gardener
