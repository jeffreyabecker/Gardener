#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "core/Units.h"
#include "panel/page/NumberScene.h"

namespace gardener::panel::page {

// The target pressure's editor: a pressure is kPa in the machine and the panel's unit on the glass,
// so a keypress is a whole step of the unit showing — one psi, one inHg, ten mmHg — and the value is
// printed with the unit's own decimals, while what the page hands back is the kPa the machine
// stores.
//
// A pressure declares no bounds of its own, so the machine's are the bounds: a keypress cannot leave
// the editor on a target `vacuum target` would refuse. A nullable setting's nothing sits one step
// below the shallowest target, where a depth begins.
class PressureScene : public NumberScene {
 public:
  // `currentKpa` is the target stored now and `wholeSteps` how many whole steps of the panel's unit
  // one keypress takes.
  PressureScene(GestureQueue& gestures, Ux& ux, String& label,
                float currentKpa, DisplayUnit unit, float wholeSteps, bool nullable);
};

}  // namespace gardener::panel::page
