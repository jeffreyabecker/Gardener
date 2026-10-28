#include "Build.h"

#if GARDENER_PANEL

#include "routines/PressureRow.h"

#include <math.h>

#include "Machine.h"

namespace gardener {

PressureRow::PressureRow(gfx::ui::GestureQueue& gestures, gfx::ui::Ux& ux, const String& key,
                         const String& label, float currentKpa, DisplayUnit unit, float wholeSteps,
                         bool nullable)
    : NumberRow(gestures, ux, key, label, currentKpa,
                wholeSteps * displayUnitInfo(unit).wholeStep / displayUnitInfo(unit).perKpa,
                CONTROL_MIN_TARGET_KPA, CONTROL_FLOOR_KPA, nullable,
                [unit](float pressureKpa) { return displayText(pressureKpa, unit); }),
      unit_(unit),
      wholeSteps_(wholeSteps) {}

std::unique_ptr<gfx::ui::Scene> PressureRow::scene(const std::any& value) {
  const float currentKpa = value.has_value() ? std::any_cast<float>(value) : NAN;
  return std::make_unique<PressureScene>(gestures_, ux_, label_, currentKpa, unit_, wholeSteps_,
                                         nullable_);
}

}  // namespace gardener

#endif
