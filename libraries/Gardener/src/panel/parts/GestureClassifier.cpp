#include "Build.h"

#if GARDENER_PANEL

#include "panel/parts/GestureClassifier.h"

namespace gardener {

GestureClassifier::GestureClassifier(ButtonPanel& buttons) : buttons_(buttons) {
  // A key held at boot is not a press and never joins a gesture: it was down before the first
  // beat, so the reader will never classify a press for it.
  for (uint8_t i = 0; i < ButtonCount; i++) {
    if (buttons_.pressed((Button)i)) down_ |= bitOf((Button)i);
  }
}

String GestureClassifier::partOf(Button button, Press press) {
  String part = buttonName(button);
  part += ':';
  part += press == Press::Double ? "double" : "single";
  return part;
}

String GestureClassifier::update() {
  // A hold past the extended mark issues its whole seconds, once per second, so a caller watching
  // for a mark sees it while the key is still down. The marks come first: a key that is still down
  // is seen even when another key's press lands on the same beat, and the press waits a beat in
  // the reader.
  String marks;
  for (uint8_t i = 0; i < ButtonCount; i++) {
    const Button button = (Button)i;
    const uint32_t heldMs = buttons_.heldMs(button);
    if (heldMs == 0) {
      // The release of a hold that already said its seconds is the mark, not a press of its own.
      if (markedSecond_[i] > 0) buttons_.consumePress(button);
      markedSecond_[i] = 0;
      continue;
    }
    const uint32_t seconds = heldMs / 1000;
    if (seconds * 1000 < ExtendedHoldMs || seconds <= markedSecond_[i]) continue;
    markedSecond_[i] = seconds;
    // The mark is this key's gesture, so it stops waiting in a group with the other keys.
    group_ &= ~bitOf(button);
    done_[i] = false;
    part_[i] = "";
    if (!marks.isEmpty()) marks += '+';
    marks += buttonName(button);
    marks += ':';
    marks += seconds;
  }
  if (!marks.isEmpty()) return marks;

  // The keys whose presses belong to the gesture being composed. A key that comes down while
  // another is down joins the same gesture, so a pair is one gesture rather than two.
  uint32_t down = 0;
  for (uint8_t i = 0; i < ButtonCount; i++) {
    if (buttons_.pressed((Button)i)) down |= bitOf((Button)i);
  }
  const uint32_t arrived = down & ~down_;
  if (arrived != 0) {
    group_ |= arrived;
    if ((down & (down - 1)) != 0) group_ |= down;
  }
  down_ = down;

  // The presses that completed, in panel order. Each says its part once, and a hold that already
  // issued its marks is not a second gesture.
  for (uint8_t i = 0; i < ButtonCount; i++) {
    const Button button = (Button)i;
    if (!(group_ & bitOf(button)) || done_[i]) continue;
    const Press press = buttons_.consumePress(button);
    if (press == Press::None) continue;
    done_[i] = true;
    part_[i] = partOf(button, press);

    // A key that is down past its mark has already said its gesture, so a press beside it joins
    // that gesture rather than waiting for both keys to come up: `back:3+cancel:single`.
    String joined;
    for (uint8_t k = 0; k < ButtonCount; k++) {
      if (markedSecond_[k] == 0) continue;
      const uint32_t heldMs = buttons_.heldMs((Button)k);
      if (heldMs == 0) continue;
      if (!joined.isEmpty()) joined += '+';
      joined += buttonName((Button)k);
      joined += ':';
      joined += heldMs / 1000;
    }
    if (!joined.isEmpty()) {
      joined += '+';
      joined += part_[i];
      part_[i] = "";
      done_[i] = false;
      group_ &= ~bitOf(button);
      return joined;
    }
  }

  // The gesture is whole once none of its keys is down and every one has said its part.
  if (group_ == 0 || (down & group_) != 0) return "";
  for (uint8_t i = 0; i < ButtonCount; i++) {
    if ((group_ & bitOf((Button)i)) && !done_[i]) return "";
  }

  String gesture;
  for (uint8_t i = 0; i < ButtonCount; i++) {
    const uint32_t bit = bitOf((Button)i);
    if (!(group_ & bit)) continue;
    if (!part_[i].isEmpty()) {
      if (!gesture.isEmpty()) gesture += '+';
      gesture += part_[i];
    }
    part_[i] = "";
    done_[i] = false;
  }
  group_ = 0;
  return gesture;
}

}  // namespace gardener
#endif
