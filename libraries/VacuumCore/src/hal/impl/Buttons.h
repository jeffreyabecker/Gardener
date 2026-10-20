#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "hal/Button.h"
#include "hal/ButtonPanel.h"

namespace gardener {

// The front panel keys: four active low switches with pull ups on the module, read through GPIO
// change interrupts.
//
// The constructor arms the pins it is given, and from then on the ISR samples them and hands the
// reader one packed mask: an edge is captured even if the loop is busy, and nothing polls a pin.
// That mask has one writer, the ISR, and an aligned 32-bit store is atomic on this part, so the
// reader takes it without a lock; the debounce settles on the read, so the class needs no service
// call of its own.
class Buttons : public ButtonPanel {
 public:
  Buttons(uint8_t up, uint8_t down, uint8_t ok, uint8_t back);

  bool pressed(Button button) override;
  Press consumePress(Button button) override;
  void dropPress(Button button) override;
  uint32_t heldMs(Button button) override;

 private:
  static void ARDUINO_ISR_ATTR isr(void* arg);
  uint32_t ARDUINO_ISR_ATTR readPins() const;
  static constexpr uint32_t bitOf(Button button) { return 1u << (uint8_t)button; }

  // Takes the level the ISR left and accepts it once it has held still.
  uint32_t settle();

  const uint8_t upPin_;
  const uint8_t downPin_;
  const uint8_t okPin_;
  const uint8_t backPin_;

  // Written only by the ISR, read by settle().
  volatile uint32_t raw_ = 0;
  // The reader's copy: the level it last saw, and when that level first appeared.
  uint32_t pending_ = 0;
  uint32_t stable_ = 0;
  uint32_t pendingSinceMs_ = 0;
  // The keys with a press in progress: set when a debounced down edge is accepted, cleared by the
  // release that classifies it. A key held at boot is never tracked, so it is not a press.
  uint32_t tracked_ = 0;
  uint32_t pressStartMs_[4] = {};
  Press pendingPress_[4] = {};
  // A short release waits out the double window before it is ready; a press that arrives inside
  // that window is marked as the second half and decides whether the pair is a double.
  uint32_t awaitingSecond_ = 0;
  uint32_t second_ = 0;
  uint32_t firstReleaseMs_[4] = {};
};

}  // namespace gardener
