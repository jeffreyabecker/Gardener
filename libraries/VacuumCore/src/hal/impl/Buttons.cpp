#include "hal/impl/Buttons.h"

#include "core/Config.h"

namespace gardener {
namespace {

// A mechanical key bounces for a few milliseconds, so a level has to hold this long before a read
// believes it. Short enough that a press still feels immediate, long enough to swallow the edges
// one press produces.
constexpr uint32_t kDebounceMs = 20;

}  // namespace

Buttons::Buttons() {
  // The module carries pull ups; the internal ones are the fallback when a key is fitted without
  // it, and either way the pin reads high until the switch pulls it to ground.
  pinMode(cfg::BTN_UP, INPUT_PULLUP);
  pinMode(cfg::BTN_DOWN, INPUT_PULLUP);
  pinMode(cfg::BTN_OK, INPUT_PULLUP);
  pinMode(cfg::BTN_BACK, INPUT_PULLUP);

  // A key held at boot is the initial level, not a press: there is nobody to have pressed it.
  stable_ = readPins();
  pending_ = stable_;
  raw_ = stable_;

  attachInterruptArg(digitalPinToInterrupt(cfg::BTN_UP), isr, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(cfg::BTN_DOWN), isr, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(cfg::BTN_OK), isr, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(cfg::BTN_BACK), isr, this, CHANGE);
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

uint32_t ARDUINO_ISR_ATTR Buttons::readPins() {
  uint32_t mask = 0;
  if (digitalRead(cfg::BTN_UP) == LOW) mask |= bitOf(Button::Up);
  if (digitalRead(cfg::BTN_DOWN) == LOW) mask |= bitOf(Button::Down);
  if (digitalRead(cfg::BTN_OK) == LOW) mask |= bitOf(Button::Ok);
  if (digitalRead(cfg::BTN_BACK) == LOW) mask |= bitOf(Button::Back);
  return mask;
}

void ARDUINO_ISR_ATTR Buttons::isr(void* arg) {
  static_cast<Buttons*>(arg)->raw_ = readPins();
}

}  // namespace gardener
