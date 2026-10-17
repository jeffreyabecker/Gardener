#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "hal/PortMux.h"

namespace gardener {

// The keys the front panel's 5-way switch exposes to this rig.
enum class Button : uint8_t { Up, Down, Ok, Back };

inline const char* buttonName(Button button) {
  switch (button) {
    case Button::Up: return "up";
    case Button::Down: return "down";
    case Button::Ok: return "ok";
    case Button::Back: return "back";
    default: return "?";
  }
}

// The front panel keys: four active low switches with pull ups on the module, read through GPIO
// change interrupts.
//
// The constructor arms the pins, and from then on the ISR samples them and hands the reader one
// packed mask: an edge is captured even if the loop is busy, and nothing polls a pin. That mask has
// one writer, the ISR, and a read takes it in a short critical section; the debounce settles on the
// read, so the class needs no service call of its own.
class Buttons {
 public:
  Buttons();

  // The debounced level. Reading settles the debounce, so this is the level now rather than the
  // level as of some earlier pass.
  bool pressed(Button button);
  // One shot: true once per press, and reading it clears it.
  bool consumePress(Button button);

 private:
  static void ARDUINO_ISR_ATTR isr(void* arg);
  static uint32_t ARDUINO_ISR_ATTR readPins();
  static constexpr uint32_t bitOf(Button button) { return 1u << (uint8_t)button; }

  // Takes the level the ISR left and accepts it once it has held still.
  uint32_t settle();

  PortMux lock_;
  // Written only by the ISR, read by settle() under the lock.
  volatile uint32_t raw_ = 0;
  // The reader's copy: the level it last saw, and when that level first appeared.
  uint32_t pending_ = 0;
  uint32_t stable_ = 0;
  uint32_t pendingSinceMs_ = 0;
  uint32_t pressLatch_ = 0;
};

}  // namespace gardener
