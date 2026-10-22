#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "hal/Button.h"
#include "hal/ButtonPanel.h"

namespace gardener {

// The keys as gestures. It owns the panel's button reader, settles it on each beat and answers the
// gesture it detected as one string: `ok:single`, `up:single+down:single`, `back:3`. Nothing in it
// knows what a caller does with a gesture.
//
// A press is named when the reader classifies it, and keys held together are composed into one
// gesture in panel order, answered when the last of them comes up. A hold that passes the extended
// mark issues its whole seconds once per second instead of waiting for the release, so a caller
// watching for a mark sees it while the key is still down.
class GestureClassifier {
 public:
  explicit GestureClassifier(ButtonPanel& buttons);

  // The gesture this beat detected, or an empty string when it detected none.
  String update();

 private:
  static constexpr uint32_t bitOf(Button button) { return 1u << (uint8_t)button; }

  static String partOf(Button button, Press press);

  ButtonPanel& buttons_;
  // The beat's levels, and the keys whose presses belong to the gesture being composed.
  uint32_t down_ = 0;
  uint32_t group_ = 0;
  bool done_[ButtonCount] = {};
  String part_[ButtonCount];
  // The last whole second each held key has issued, so a mark goes out once.
  uint32_t markedSecond_[ButtonCount] = {};
};

}  // namespace gardener
