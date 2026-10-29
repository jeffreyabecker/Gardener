#include "Build.h"

#if GARDENER_PANEL

#include "panel/glass/PressureScene.h"

#include "machine/Machine.h"

namespace gardener::panel::glass {

// The step is the unit's whole step converted to the kPa the value is held in, so the editor moves
// by what the glass reads rather than by a kilopascal.
PressureScene::PressureScene(panel::page::GestureQueue& gestures, panel::page::Ux& ux,
                             String& label, float currentKpa, DisplayUnit unit, float wholeSteps,
                             bool nullable)
    : NumberScene(gestures, ux, label, currentKpa,
                  wholeSteps * displayUnitInfo(unit).wholeStep / displayUnitInfo(unit).perKpa,
                  CONTROL_MIN_TARGET_KPA, CONTROL_FLOOR_KPA, nullable,
                  [unit](float pressureKpa) { return displayText(pressureKpa, unit); }) {}

}  // namespace gardener::panel::glass
#endif
