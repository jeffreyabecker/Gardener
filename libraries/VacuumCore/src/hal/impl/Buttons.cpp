#include "hal/impl/Buttons.h"

namespace gardener {
namespace {

// A mechanical key bounces for a few milliseconds, so a level has to hold this long before a read
// believes it. Short enough that a press still feels immediate, long enough to swallow the edges
// one press produces.
constexpr uint32_t kDebounceMs = 20;

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

  uint32_t raw;
  lock_.enter();
  raw = raw_;
  lock_.exit();

  if (raw != pending_) {
    pending_ = raw;
    pendingSinceMs_ = now;
  } else if (pending_ != stable_ && now - pendingSinceMs_ >= kDebounceMs) {
    // The level held still, so it is the new state; a bit that went from up to down is one press.
    pressLatch_ |= pending_ & ~stable_;
    stable_ = pending_;
  }
  return stable_;
}

bool Buttons::pressed(Button button) {
  return (settle() & bitOf(button)) != 0;
}

bool Buttons::consumePress(Button button) {
  settle();
  uint32_t bit = bitOf(button);
  if (!(pressLatch_ & bit)) return false;
  pressLatch_ &= ~bit;
  return true;
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
