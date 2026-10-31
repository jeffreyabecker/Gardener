#pragma once

#include <stdint.h>

namespace gardener {
inline namespace core {

// The range the fitted piezo reproduces. The note parser rejects a frequency outside it, and the
// wire publishes it as the `tone` bounds, so it belongs to the part family rather than to a driver.
constexpr float BuzzerMinHz = 100.0f;
constexpr float BuzzerMaxHz = 10000.0f;

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

}  // namespace core
}  // namespace gardener
