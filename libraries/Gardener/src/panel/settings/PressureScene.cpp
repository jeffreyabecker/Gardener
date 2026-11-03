#include "panel/settings/PressureScene.h"

namespace gardener {

// The step is the unit's whole step converted to the kPa the value is held in, so the editor moves
// by what the glass reads rather than by a kilopascal.
PressureScene::PressureScene(GestureQueue& gestures, Ux& ux, String& label, float currentKpa,
                             DisplayUnit unit, float wholeSteps, float minKpa, float maxKpa,
                             bool nullable)
    : NumberScene(gestures, ux, label, currentKpa,
                  wholeSteps * displayUnitInfo(unit).wholeStep / displayUnitInfo(unit).perKpa,
                  minKpa, maxKpa, nullable,
                  [unit](float pressureKpa) { return displayText(pressureKpa, unit); }) {}

}  // namespace gardener