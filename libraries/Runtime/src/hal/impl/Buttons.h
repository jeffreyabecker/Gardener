#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "core/Button.h"
#include "core/ButtonPanel.h"

namespace gardener::runtime {

// The front panel keys: eight active low switches with pull ups, read through one 74HC165 shift
// register.
//
// The part has no change output, so there is no edge to capture: the register is latched and
// shifted in on every read, and a level is accepted only once it has held still. The panel task
// reads on a 10 ms beat, which gives the 20 ms debounce two or three samples of a settled level.
//
// Which input a key sits on is the board's fact rather than the panel's, so the constructor takes
// it: a mask indexed by Button, holding that key's bit, with bit 0 the input read before the first
// clock. The rest of the classification — the double window, the hold, the key held at boot — is
// the same as it was on the pins, only fed from a shift instead of a digitalRead each.
class ShiftRegisterButtons : public ButtonPanel {
 public:
  ShiftRegisterButtons(uint8_t latch, uint8_t clock, uint8_t data,
                       const uint8_t (&buttonMask)[ButtonCount]);

  bool pressed(Button button) override;
  Press consumePress(Button button) override;
  void dropPress(Button button) override;
  uint32_t heldMs(Button button) override;

 private:
  uint32_t bitOf(Button button) const { return buttonMask_[(uint8_t)button]; }

  // Shifts the eight inputs in, one bit per key, with the mask naming the key on each bit.
  uint32_t readKeys();
  // Takes the level the register gave back and accepts it once it has held still.
  uint32_t settle();

  const uint8_t latchPin_;
  const uint8_t clockPin_;
  const uint8_t dataPin_;
  // The board's own table, which is a rig constant and outlives the reader.
  const uint8_t (&buttonMask_)[ButtonCount];

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

}  // namespace gardener::runtime