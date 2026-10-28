#include "Build.h"

#if GARDENER_PANEL

#include "routines/PressureRow.h"

#include <math.h>

#include <utility>

#include "machine/Machine.h"

namespace gardener {

PressureRow::PressureRow(gfx::ui::GestureQueue& gestures, gfx::ui::Ux& ux, const String& key,
                         const String& label, float currentKpa, DisplayUnit unit, float wholeSteps,
                         bool nullable, gfx::ui::Validator validator)
    : NumberRow(gestures, ux, key, label, currentKpa,
                wholeSteps * displayUnitInfo(unit).wholeStep / displayUnitInfo(unit).perKpa,
                CONTROL_MIN_TARGET_KPA, CONTROL_FLOOR_KPA, nullable,
                [unit](float pressureKpa) { return displayText(pressureKpa, unit); }, 2,
                std::move(validator)),
      unit_(unit),
      wholeSteps_(wholeSteps) {}

// The pressure's number row, under the kind its declaration named. The step and bounds are the
// number's own — the step one keypress makes in the unit the setting stores, and the machine's floor
// and ceiling — because those are what the editor it builds moves within.
gfx::ui::SettingDeclaration PressureRow::declaration() const {
  gfx::ui::SettingDeclaration out = NumberRow::declaration();
  out.kind = "pressure";
  return out;
}

std::unique_ptr<gfx::ui::EditorScene> PressureRow::scene(const std::any& value) {
  const float currentKpa = value.has_value() ? std::any_cast<float>(value) : NAN;
  return std::make_unique<PressureScene>(gestures_, ux_, label_, currentKpa, unit_, wholeSteps_,
                                         nullable_);
}

}  // namespace gardener

#endif
