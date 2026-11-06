#pragma once

#include <stdint.h>

#include "core/Knob.h"

namespace gardener {

// The range the fitted piezo reproduces. The note parser rejects a frequency outside it, and the
// wire publishes it as the `tone` bounds, so it belongs to the part family rather than to a driver.
GARDENER_KNOB_VALID(float, BuzzerMinHz, GARDENER_BUZZER_MIN_HZ, 100.0f,
                    GARDENER_BUZZER_MIN_HZ > 0.0f, "a piezo's range starts above zero");
GARDENER_KNOB_VALID(float, BuzzerMaxHz, GARDENER_BUZZER_MAX_HZ, 10000.0f,
                    GARDENER_BUZZER_MAX_HZ > GARDENER_BUZZER_MIN_HZ &&
                        GARDENER_BUZZER_MAX_HZ <= 20000.0f,
                    "the range is a range, and a piezo stops at 20 kHz");

// The piezo: one square wave output. The part knows frequency and silence, not durations or note
// names — `BuzzerPlayer` owns what to play and for how long.
//
// `sounding()` and `frequencyHz()` report the level the part is actually driving, so a reader does
// not keep a second copy of the current tone.
class Buzzer {
 public:
  virtual ~Buzzer() = default;

  // False when the PWM channel could not be attached, so the caller can say so at boot.
  virtual bool begin() = 0;

  // A 50 % square wave at `hz`. Zero is silence.
  virtual void tone(uint16_t hz) = 0;
  virtual void off() = 0;

  virtual bool sounding() const = 0;
  virtual uint16_t frequencyHz() const = 0;
};

}  // namespace gardener