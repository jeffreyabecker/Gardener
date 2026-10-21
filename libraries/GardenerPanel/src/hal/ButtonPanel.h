#pragma once

#include "hal/Button.h"

namespace gardener {

// The front panel keys, as the rest of the firmware sees them. The GPIO and the interrupts are in
// hal/impl/, where the pins it is given are armed.
class ButtonPanel {
 public:
  virtual ~ButtonPanel() = default;

  // The debounced level. Reading settles the debounce, so this is the level now rather than the
  // level as of some earlier pass.
  virtual bool pressed(Button button) = 0;
  // One shot: the press this key completed since the last read, or None. A press is classified
  // when the key comes up, so a held key has no event until it is released. A short press is held
  // back for the double window, so a double is reported once rather than as two singles.
  virtual Press consumePress(Button button) = 0;
  // Discard a key's pending press: its classified press and any double window still open. The
  // panel task uses it for the Up+Down chord, whose two keys are one gesture rather than two
  // presses.
  virtual void dropPress(Button button) = 0;
  // How long the key has been held, or 0 when it is up. This is the running hold a consumer watches
  // when it cares about a hold longer than a press.
  virtual uint32_t heldMs(Button button) = 0;
};

}  // namespace gardener
