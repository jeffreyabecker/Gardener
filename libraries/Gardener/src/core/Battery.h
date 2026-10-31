#pragma once

#include <stdint.h>

namespace gardener {
inline namespace core {

// The pack gauge: a readout, not a protection. Nothing in the firmware acts on a low reading
// (Q11), the icon and the battery keys are the whole story. percent() is a straight line between
// the configured empty and full volts, which is good enough for a bar and honest about being
// +/-3-5 % at best.
//
// This is the interface the rest of the firmware holds. The divider on an ADC pin is in
// hal/impl/, along with the calibration; the pin itself comes from the composition root.
class Battery {
 public:
  virtual ~Battery() = default;

  // False when the sense pin could not be configured, so the caller can say so at boot.
  virtual bool begin() = 0;
  virtual void update(uint32_t nowMs) = 0;

  virtual bool present() const = 0;
  virtual bool low() const = 0;
  virtual float volts() const = 0;
  virtual uint8_t percent() const = 0;
};

}  // namespace core
}  // namespace gardener
