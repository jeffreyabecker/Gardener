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
  // One shot: true once per press, and reading it clears it.
  virtual bool consumePress(Button button) = 0;
};

}  // namespace gardener
