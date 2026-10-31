#pragma once

#include <stdint.h>

namespace gardener {
inline namespace core {

// Where the vacuum system gets pressure from: how far below ambient the chamber is, in kPa.
//
// This is the interface the vacuum system holds. The part on this rig — a CFSensor XGZP6847A, whose
// amplified analog output is read by an ADC pin and averaged before it is filtered — is in
// hal/impl/.
//
// `pressureKpa` is the vacuum depth: zero at ambient and rising as the chamber is pulled down. The part
// takes its own reference when it comes up, in begin(), so the depth is measured below whatever
// pressure the rig was at when it booted. Which part does that, and how, stays the
// implementation's business.
class PressureSensor {
 public:
  virtual ~PressureSensor() = default;

  // False when the part did not come up, so the caller can say so at boot and carry on.
  virtual bool begin() = 0;
  virtual void update(uint32_t nowMs) = 0;

  // False when the part has stopped reporting something plausible, which mid-run is a fault.
  virtual bool present() const = 0;
  // False until a sample has been taken and filtered; regulating on one is how a loop runs away.
  virtual bool readingValid() const = 0;

  // Vacuum depth in kPa, from the reference the part took when it came up. Zero while
  // readingValid() is false.
  virtual float pressureKpa() const = 0;
};

}  // namespace core
}  // namespace gardener
