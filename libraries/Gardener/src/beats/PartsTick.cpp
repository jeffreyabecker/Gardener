#include "beats/PartsTick.h"

#include "core/Battery.h"
#include "core/SystemState.h"
#include "panel/parts/BuzzerPlayer.h"

namespace gardener {
inline namespace beats {

PartsTick::PartsTick(Battery& battery, panel::parts::BuzzerPlayer& buzzer, SystemState& state)
    : battery_(battery), buzzer_(buzzer), state_(state) {}

void PartsTick::tick(uint32_t nowMs) {
  battery_.update(nowMs);
  buzzer_.update(nowMs);

  // The pack's slice, read once at the end of the beat. No gauge behind the reading means no
  // percentage rather than whatever the part last held.
  state_.batteryPresent = battery_.present();
  state_.batteryPercent = state_.batteryPresent ? battery_.percent() : 0;
  state_.batteryLow = battery_.low();
}

}  // namespace beats
}  // namespace gardener
