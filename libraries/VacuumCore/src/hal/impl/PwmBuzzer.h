#pragma once

#include <stdint.h>

#include "hal/Buzzer.h"

namespace gardener {

// The rig's piezo: a passive element on one GPIO through an LEDC channel. The channel's setup is
// here because it belongs to the drive; what to play is the player's business.
class PwmBuzzer : public Buzzer {
 public:
  explicit PwmBuzzer(uint8_t pin) : pin_(pin) {}

  bool begin() override;

  void tone(uint16_t hz) override;
  void off() override;

  bool sounding() const override { return sounding_; }
  uint16_t frequencyHz() const override { return hz_; }

 private:
  const uint8_t pin_;
  uint16_t hz_ = 0;
  bool sounding_ = false;
};

}  // namespace gardener
