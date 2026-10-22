#include "hal/impl/Buttons.h"

namespace gardener {
namespace {

// A mechanical key bounces for a few milliseconds, so a level has to hold this long before a read
// believes it. Short enough that a press still feels immediate, long enough to swallow the edges
// one press produces.
constexpr uint32_t DebounceMs = 20;

// Two singles with the second starting within this of the first release are one double. The first
// is held back for the window so it cannot also act as a single. A hold past ExtendedHoldMs is not
// a press at all, so it never joins a double.
constexpr uint32_t DoublePressMs = 350;

}  // namespace

Buttons::Buttons(uint8_t latch, uint8_t clock, uint8_t data)
    : latchPin_(latch), clockPin_(clock), dataPin_(data) {
  // Latch high and clock low is the shift state: a low pulse on the latch loads the parallel
  // inputs, and every clock after it moves the next one out on Q7.
  pinMode(latchPin_, OUTPUT);
  pinMode(clockPin_, OUTPUT);
  digitalWrite(latchPin_, HIGH);
  digitalWrite(clockPin_, LOW);
  // The register drives Q7, so the pin is a plain input: there is nothing to pull up.
  pinMode(dataPin_, INPUT);

  // A key held at boot is the initial level, not a press: there is nobody to have pressed it.
  stable_ = readKeys();
  pending_ = stable_;
}

uint32_t Buttons::readKeys() {
  digitalWrite(latchPin_, LOW);
  digitalWrite(latchPin_, HIGH);

  // The part presents H first and shifts towards A, so the bit read before each clock is the next
  // key in panel order. The two spare inputs are never read.
  uint32_t mask = 0;
  for (uint8_t i = 0; i < ButtonCount; i++) {
    if (digitalRead(dataPin_) == LOW) mask |= 1u << i;
    digitalWrite(clockPin_, HIGH);
    digitalWrite(clockPin_, LOW);
  }
  return mask;
}

uint32_t Buttons::settle() {
  uint32_t now = millis();

  const uint32_t raw = readKeys();

  if (raw != pending_) {
    pending_ = raw;
    pendingSinceMs_ = now;
  } else if (pending_ != stable_ && now - pendingSinceMs_ >= DebounceMs) {
    // The level held still, so it is the new state. A key that came up after its down edge was
    // accepted is the one press to classify; a key held at boot has no down edge and is not one.
    const uint32_t down = pending_ & ~stable_;
    const uint32_t up = stable_ & ~pending_;
    for (uint8_t i = 0; i < ButtonCount; i++) {
      const uint32_t bit = 1u << i;
      if (down & bit) {
        tracked_ |= bit;
        // The raw edge, not the moment the debounce accepted it, so a hold is measured from when
        // the key actually went down.
        pressStartMs_[i] = pendingSinceMs_;
        // A press inside the window is the second half of a double; the first short press stays
        // pending until this one resolves.
        if (awaitingSecond_ & bit) {
          awaitingSecond_ &= ~bit;
          second_ |= bit;
        } else {
          second_ &= ~bit;
        }
      }
      if ((up & bit) && (tracked_ & bit)) {
        // Both ends are the raw edges, so the debounce does not shorten or lengthen the hold.
        const uint32_t held = pendingSinceMs_ - pressStartMs_[i];
        if (second_ & bit) {
          pendingPress_[i] = Press::Double;
          second_ &= ~bit;
        } else {
          pendingPress_[i] = Press::Single;
          // A hold past the mark has said its seconds already, so it waits no double window.
          if (held < ExtendedHoldMs) {
            awaitingSecond_ |= bit;
            firstReleaseMs_[i] = pendingSinceMs_;
          }
        }
        tracked_ &= ~bit;
      }
    }
    stable_ = pending_;
  }

  for (uint8_t i = 0; i < ButtonCount; i++) {
    const uint32_t bit = 1u << i;
    // A single is ready once the double window closes without a second press.
    if ((awaitingSecond_ & bit) && now - firstReleaseMs_[i] >= DoublePressMs) {
      awaitingSecond_ &= ~bit;
    }
    // A second press held past the mark is a hold, not half a double: release the first single now
    // rather than waiting for the second to come up.
    if ((second_ & bit) && (tracked_ & bit) && now - pressStartMs_[i] >= ExtendedHoldMs) {
      second_ &= ~bit;
    }
  }
  return stable_;
}

bool Buttons::pressed(Button button) {
  return (settle() & bitOf(button)) != 0;
}

Press Buttons::consumePress(Button button) {
  settle();
  // The first single of a possible double is not ready until the pair is ruled out.
  if ((awaitingSecond_ | second_) & bitOf(button)) return Press::None;
  const uint8_t index = (uint8_t)button;
  const Press press = pendingPress_[index];
  pendingPress_[index] = Press::None;
  return press;
}

// The chord's keys are one gesture, so their presses are dropped rather than delivered: the
// classified press and the double window it may still be waiting in.
void Buttons::dropPress(Button button) {
  const uint8_t index = (uint8_t)button;
  const uint32_t bit = bitOf(button);
  pendingPress_[index] = Press::None;
  awaitingSecond_ &= ~bit;
  second_ &= ~bit;
}

uint32_t Buttons::heldMs(Button button) {
  settle();
  if (!(tracked_ & bitOf(button))) return 0;
  return millis() - pressStartMs_[(uint8_t)button];
}

}  // namespace gardener
