#pragma once

#include <stdint.h>

namespace eggplan {

// Optional open collector tach or hall output. Disabled when the pin is negative,
// which keeps the ISR and the pin configuration out of the way until a wire is
// fitted.
class Tachometer {
 public:
  void begin(int pin, uint8_t pulsesPerRev);
  void update(uint32_t nowMs);

  bool enabled() const { return pin_ >= 0; }
  float rpm() const { return rpm_; }

 private:
  static void isr(void* arg);

  int pin_ = -1;
  uint8_t pulsesPerRev_ = 1;
  volatile uint32_t pulses_ = 0;
  uint32_t windowStartMs_ = 0;
  uint32_t windowPulses_ = 0;
  float rpm_ = 0.0f;
};

}  // namespace eggplan
