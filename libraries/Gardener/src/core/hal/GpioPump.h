#pragma once

#include <stdint.h>

#include "core/Pump.h"

namespace gardener {

// A pump on one GPIO through a low side MOSFET, driven active high because the MOSFET gate is.
// There is no duty and no ramp: the loop switches the pump and the chamber does the rest, so the
// only fact this part holds is where the pin was last driven.
class GpioPump : public Pump {
 public:
  explicit GpioPump(uint8_t pin) : pin_(pin) {}

  void begin() override;
  void on() override;
  void off() override;

  bool active() const override { return on_; }

 private:
  const uint8_t pin_;
  bool on_ = false;
};

}  // namespace gardener
