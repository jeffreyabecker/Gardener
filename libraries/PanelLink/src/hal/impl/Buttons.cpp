#include "hal/impl/Buttons.h"

namespace gardener {
namespace {

// A mechanical key bounces for a few milliseconds, so a level has to hold this long before a read
// believes it. Short enough that a press still feels immediate, long enough to swallow the edges
// one press produces.
constexpr uint32_t kDebounceMs = 20;

// A key released before this is a short press; at or after it, a long press. Both are reported when
// the key comes up. A consumer that wants a longer hold watches heldMs() and picks its own mark, so
// there is no third threshold here.
constexpr uint32_t LongPressMs = 600;
// Two short presses with the second starting within this of the first release are one double
// press. The first is held back for the window so it cannot also act as a single; a long press is
// never part of a double.
constexpr uint32_t DoublePressMs = 350;

}  // namespace

Buttons::Buttons(uint8_t up, uint8_t down, uint8_t ok, uint8_t back)
    : upPin_(up), downPin_(down), okPin_(ok), backPin_(back) {
  // The module carries pull ups; the internal ones are the fallback when a key is fitted without
  // it, and either way the pin reads high until the switch pulls it to ground.
  pinMode(upPin_, INPUT_PULLUP);
  pinMode(downPin_, INPUT_PULLUP);
  pinMode(okPin_, INPUT_PULLUP);
  pinMode(backPin_, INPUT_PULLUP);

  // A key held at boot is the initial level, not a press: there is nobody to have pressed it.
  stable_ = readPins();
  pending_ = stable_;
  raw_ = stable_;

  attachInterruptArg(digitalPinToInterrupt(upPin_), isr, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(downPin_), isr, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(okPin_), isr, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(backPin_), isr, this, CHANGE);
}

uint32_t Buttons::settle() {
  uint32_t now = millis();

  const uint32_t raw = raw_;

  if (raw != pending_) {
    pending_ = raw;
    pendingSinceMs_ = now;
  } else if (pending_ != stable_ && now - pendingSinceMs_ >= kDebounceMs) {
    // The level held still, so it is the new state. A key that came up after its down edge was
    // accepted is the one press to classify; a key held at boot has no down edge and is not one.
    const uint32_t down = pending_ & ~stable_;
    const uint32_t up = stable_ & ~pending_;
    for (uint8_t i = 0; i < 4; i++) {
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
        const bool shortPress = held < LongPressMs;
        if (second_ & bit) {
          pendingPress_[i] = shortPress ? Press::Double : Press::Long;
          second_ &= ~bit;
        } else {
          pendingPress_[i] = shortPress ? Press::Short : Press::Long;
          if (shortPress) {
            awaitingSecond_ |= bit;
            firstReleaseMs_[i] = pendingSinceMs_;
          }
        }
        tracked_ &= ~bit;
      }
    }
    stable_ = pending_;
  }

  for (uint8_t i = 0; i < 4; i++) {
    const uint32_t bit = 1u << i;
    // A single short press is ready once the double window closes without a second press.
    if ((awaitingSecond_ & bit) && now - firstReleaseMs_[i] >= DoublePressMs) {
      awaitingSecond_ &= ~bit;
    }
    // A second press that becomes long is a long press, not half a double: release the first short
    // now rather than waiting for the second to come up.
    if ((second_ & bit) && (tracked_ & bit) && now - pressStartMs_[i] >= LongPressMs) {
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
  // The first short of a possible double is not ready until the pair is ruled out.
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

uint32_t ARDUINO_ISR_ATTR Buttons::readPins() const {
  uint32_t mask = 0;
  if (digitalRead(upPin_) == LOW) mask |= bitOf(Button::Up);
  if (digitalRead(downPin_) == LOW) mask |= bitOf(Button::Down);
  if (digitalRead(okPin_) == LOW) mask |= bitOf(Button::Ok);
  if (digitalRead(backPin_) == LOW) mask |= bitOf(Button::Back);
  return mask;
}

void ARDUINO_ISR_ATTR Buttons::isr(void* arg) {
  auto* self = static_cast<Buttons*>(arg);
  self->raw_ = self->readPins();
}

}  // namespace gardener
