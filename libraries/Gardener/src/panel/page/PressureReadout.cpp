#include "panel/page/PressureReadout.h"

#include <Arduino.h>

#include <utility>

#include "layout/Drawing.h"
#include "layout/Layout.h"
#include "shapes/Text.h"

namespace gardener {
namespace {

// A reading as the panel prints it: the unit's scale, and the decimals the unit's fine step can
// express.
String readingText(float pressureKpa, DisplayUnit unit) {
  char out[16];
  snprintf(out, sizeof(out), "%.*f", static_cast<int>(displayDecimals(unit)),
           displayValue(pressureKpa, unit));
  return out;
}

}  // namespace

gfx::PaintNodeRef pressureReadout(gfx::Box box, const SystemState& state,
                                  const gfx::Family* family) {
  return gfx::callable([box, state, family](gfx::Canvas& canvas) {
    // A stopped machine has no chamber reading to show, so the reading is dashes; the target is
    // known either way.
    const String reading =
        state.running ? readingText(state.currentPressureKpa, state.unit) : String("--");
    const String target = readingText(state.targetPressureKpa, state.unit);

    // The two readings are two parts of the box rather than two boxes over the same glass, which is
    // what keeps a reading as wide as the box from being drawn through the target: each fills its
    // own part and the resolver narrows what does not fit.
    gfx::View own = canvas.child(box.in(canvas));
    const int16_t height = own.height();
    const int16_t top = static_cast<int16_t>(height * 3 / 5);
    gfx::text(gfx::Box(0, 0, own.width(), top), reading, family)->draw(own);
    gfx::text(gfx::Box(0, top, own.width(), static_cast<int16_t>(height - top)), target, family,
              gfx::Justify::Center, gfx::Justify::End)
        ->draw(own);
  });
}

}  // namespace gardener