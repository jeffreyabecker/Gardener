#include "Build.h"

#if GARDENER_PANEL

#include "panel/parts/PartsTick.h"

#include "core/Battery.h"
#include "panel/parts/BuzzerPlayer.h"

namespace gardener::panel::parts {

PartsTick::PartsTick(Battery& battery, BuzzerPlayer& buzzer) : battery_(battery), buzzer_(buzzer) {}

void PartsTick::tick(uint32_t nowMs) {
  battery_.update(nowMs);
  buzzer_.update(nowMs);
}

}  // namespace gardener::panel::parts
#endif
