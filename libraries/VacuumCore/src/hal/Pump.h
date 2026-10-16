#pragma once

#include <stdint.h>

namespace gardener {

// The pump: the one actuator the control loop has, so the plant can pull down and never vent.
//
// This is the interface the plant holds, in the same shape as Display: what the loop needs a pump
// to do, with no pins and no PWM in it. What drives a pump on this rig — an LEDC channel into a low
// side MOSFET — is in hal/impl/, along with the duty cap and the slew limit, because those are
// properties of the part rather than of the loop.
class Pump {
 public:
  virtual ~Pump() = default;

  // False when the PWM channel could not be attached, so the caller can say so at boot.
  virtual bool begin() = 0;
  // Ramps the output toward whatever duty was last requested.
  virtual void update(uint32_t nowMs) = 0;

  // The loop's request, in percent. Zero is a request, not a stop: it ramps down like any other.
  virtual void setDuty(float percent) = 0;
  virtual void stop() = 0;
  // Cut the output now, without the ramp. What a fault asks for.
  virtual void halt() = 0;

  virtual float dutyPercent() const = 0;
};

}  // namespace gardener
