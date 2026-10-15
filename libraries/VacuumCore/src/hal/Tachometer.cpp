#include "hal/Tachometer.h"

#include <Arduino.h>

namespace eggplan {
namespace {
constexpr uint32_t kWindowMs = 500;
}

void Tachometer::begin(int pin, uint8_t pulsesPerRev) {
  pin_ = pin;
  pulsesPerRev_ = pulsesPerRev > 0 ? pulsesPerRev : 1;
  if (pin_ < 0) return;
  pinMode(pin_, INPUT_PULLUP);
  attachInterruptArg(digitalPinToInterrupt(pin_), &Tachometer::isr, this, FALLING);
}

void Tachometer::isr(void* arg) {
  Tachometer* self = static_cast<Tachometer*>(arg);
  self->pulses_++;
}

void Tachometer::update(uint32_t nowMs) {
  if (pin_ < 0) return;
  uint32_t windowMs = nowMs - windowStartMs_;
  if (windowMs < kWindowMs) return;

  noInterrupts();
  uint32_t pulses = pulses_;
  interrupts();

  uint32_t delta = pulses - windowPulses_;
  windowPulses_ = pulses;
  float revolutions = (float)delta / (float)pulsesPerRev_;
  rpm_ = revolutions * (60000.0f / (float)windowMs);
  windowStartMs_ = nowMs;
}

}  // namespace eggplan
