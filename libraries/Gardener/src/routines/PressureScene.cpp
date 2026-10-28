#include "Build.h"

#if GARDENER_PANEL

#include "routines/PressureScene.h"

#include <stdio.h>

#include "Machine.h"

namespace gardener {
namespace {

// A target as the panel prints it: the unit's scale, and the decimals the unit's fine step can
// express.
String displayText(float pressureKpa, DisplayUnit unit) {
  char printed[16];
  snprintf(printed, sizeof(printed), "%.*f", static_cast<int>(displayDecimals(unit)),
           displayValue(pressureKpa, unit));
  return String(printed);
}

}  // namespace

// The step is the unit's whole step converted to the kPa the value is held in, so the editor moves
// by what the glass reads rather than by a kilopascal.
PressureScene::PressureScene(gfx::ui::GestureQueue& gestures, gfx::ui::Ux& ux, String& label,
                             float currentKpa, DisplayUnit unit, float wholeSteps, bool nullable)
    : NumberScene(gestures, ux, label, currentKpa,
                  wholeSteps * displayUnitInfo(unit).wholeStep / displayUnitInfo(unit).perKpa,
                  CONTROL_MIN_TARGET_KPA, CONTROL_FLOOR_KPA, nullable,
                  [unit](float pressureKpa) { return displayText(pressureKpa, unit); }) {}

}  // namespace gardener
#endif
