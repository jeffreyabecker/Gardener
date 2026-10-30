#pragma once

#include <stdint.h>

namespace gardener {

class Battery;

}  // namespace gardener

namespace gardener {

struct SystemState;

}  // namespace gardener

namespace gardener::panel::parts {

class BuzzerPlayer;

// The panel's own parts' beat: the pack gauge and the piezo, updated on the one millisecond the
// panel task hands a beat. Nothing here blocks or owns a clock.
class PartsTick {
 public:
  PartsTick(Battery& battery, BuzzerPlayer& buzzer, SystemState& state);

  // One beat for the fitted parts.
  void tick(uint32_t nowMs);

 private:
  Battery& battery_;
  BuzzerPlayer& buzzer_;
  // The pack's slice of the published state, written at the end of each beat.
  SystemState& state_;
};

}  // namespace gardener::panel::parts
