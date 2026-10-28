#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "hal/Button.h"
#include "hal/ButtonPanel.h"

namespace gardener {

// The front panel keys: six active low switches with pull ups, read through one 74HC165 shift
// register.
//
// The part has no change output, so there is no edge to capture: the register is latched and
// shifted in on every read, and a level is accepted only once it has held still. The panel task
// reads on a 10 ms beat, which gives the 20 ms debounce two or three samples of a settled level.
//
// Six of the register's eight inputs are the keys in panel order and the two spares are tied high;
// the rest of the classification — the double window, the hold, the key held at boot — is the
// same as it was on the pins, only fed from a shift instead of a digitalRead each.
class Buttons : public ButtonPanel {
 public:
  Buttons(uint8_t latch, uint8_t clock, uint8_t data);

  bool pressed(Button button) override;
  Press consumePress(Button button) override;
  void dropPress(Button button) override;
  uint32_t heldMs(Button button) override;

 private:
  static constexpr uint32_t bitOf(Button button) { return 1u << (uint8_t)button; }

  // Shifts the eight inputs in and takes the six keys from them.
  uint32_t readKeys();
  // Takes the level the register gave back and accepts it once it has held still.
  uint32_t settle();

  const uint8_t latchPin_;
  const uint8_t clockPin_;
  const uint8_t dataPin_;

  // The reader's copy: the level it last saw, and when that level first appeared.
  uint32_t pending_ = 0;
  uint32_t stable_ = 0;
  uint32_t pendingSinceMs_ = 0;
  // The keys with a press in progress: set when a debounced down edge is accepted, cleared by the
  // release that classifies it. A key held at boot is never tracked, so it is not a press.
  uint32_t tracked_ = 0;
  uint32_t pressStartMs_[ButtonCount] = {};
  Press pendingPress_[ButtonCount] = {};
  // A short release waits out the double window before it is ready; a press that arrives inside
  // that window is marked as the second half and decides whether the pair is a double.
  uint32_t awaitingSecond_ = 0;
  uint32_t second_ = 0;
  uint32_t firstReleaseMs_[ButtonCount] = {};
};

}  // namespace gardener
