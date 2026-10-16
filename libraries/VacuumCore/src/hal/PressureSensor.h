#pragma once

#include <stdint.h>

namespace gardener {

// Where the plant gets pressure from: an absolute reading, and a captured ambient reference for it
// to state targets against.
//
// This is the interface the plant holds. The part on this rig — an Adafruit MPRLS on I2C, with a
// median window and an EMA over it — is in hal/impl/, which is also the only place that includes
// the vendor library.
//
// Pressures are kPa. `kpa` is the absolute reading; `vacuumKpa` and `vacuumPercent` are that
// reading measured against the reference, which is the frame a target is stated in. A reading that
// is not valid reports zero deficit rather than a guess.
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

  virtual float kpa() const = 0;
  virtual float ambientKpa() const = 0;
  virtual float vacuumKpa() const = 0;
  virtual float vacuumPercent() const = 0;

  // The reference is captured and never dialled in: this is the only way one is ever set.
  virtual void captureAmbient() = 0;
};

}  // namespace gardener
