#pragma once

#include <stdint.h>

namespace gardener {

// Where the vacuum system gets pressure from: how far below ambient the chamber is, in kPa.
//
// This is the interface the vacuum system holds. The part on this rig — an Adafruit MPRLS on I2C, with a
// median window and an EMA over it — is in hal/impl/, which is also the only place that includes
// the vendor library.
//
// `kpa` is the vacuum depth: zero at ambient and rising as the chamber is pulled down. A part that
// measures absolute pressure takes its own reference when it comes up, in begin(); a gauge part
// reads the depth directly. Which it is stays the implementation's business.
class PressureSensor {
 public:
  virtual ~PressureSensor() = default;

  // False when the part did not answer, so the caller can say so at boot and carry on.
  virtual bool begin() = 0;
  virtual void update(uint32_t nowMs) = 0;

  // False when the part has stopped answering, which mid-run is a fault.
  virtual bool present() const = 0;
  // False until a sample has been taken and filtered; regulating on one is how a loop runs away.
  virtual bool readingValid() const = 0;

  // Vacuum depth in kPa, from the reference the part took when it came up. Zero while
  // readingValid() is false.
  virtual float kpa() const = 0;
};

}  // namespace gardener
