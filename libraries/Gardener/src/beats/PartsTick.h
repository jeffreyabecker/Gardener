#pragma once

#include <stdint.h>

namespace gardener {
inline namespace core {
class Battery;
struct SystemState;
}

namespace panel {
namespace parts {
class BuzzerPlayer;
}  // namespace parts
}  // namespace panel

inline namespace beats {

// The panel's own parts' beat: the pack gauge and the piezo, updated on the one millisecond the
// panel task hands a beat. Nothing here blocks or owns a clock.
class PartsTick {
 public:
  PartsTick(Battery& battery, panel::parts::BuzzerPlayer& buzzer, SystemState& state);

  // One beat for the fitted parts.
  void tick(uint32_t nowMs);

 private:
  Battery& battery_;
  panel::parts::BuzzerPlayer& buzzer_;
  // The pack's slice of the published state, written at the end of each beat.
  SystemState& state_;
};

}  // namespace beats
}  // namespace gardener
