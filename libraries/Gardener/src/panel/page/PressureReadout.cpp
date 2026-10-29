#include "Build.h"

#if GARDENER_PANEL

#include "panel/page/PressureReadout.h"

#include <stdio.h>

#include <string>

namespace gardener::panel::page {
namespace {

// A reading as the panel prints it: the unit's scale, and the decimals the unit's fine step can
// express.
std::string readingText(float pressureKpa, DisplayUnit unit) {
  char out[16];
  snprintf(out, sizeof(out), "%.*f", static_cast<int>(displayDecimals(unit)),
           displayValue(pressureKpa, unit));
  return out;
}

}  // namespace

gfx::Paint pressureReadout(gfx::Rect box, VacuumSystem& vacuum, DisplayUnit unit,
                               float sizeHint, const gfx::Font* face, uint8_t scale,
                               uint8_t secondaryScale) {
  return [box, &vacuum, unit, sizeHint, face, scale,
          secondaryScale](gfx::Canvas& canvas) {
    // A stopped machine has no chamber reading to show, so the reading is dashes; the target is
    // known either way.
    const std::string reading =
        vacuum.running() ? readingText(vacuum.currentPressureKpa(), unit) : std::string("--");
    const std::string target = readingText(vacuum.targetPressureKpa(), unit);

    gfx::Canvas own = canvas.child(box);
    const int16_t width = own.width();
    const int16_t height = own.height();
    const int16_t halfWidth = width / 2;
    const int16_t halfHeight = height / 2;

    if (sizeHint >= 1.0f) {
      gfx::Canvas left = own.child(gfx::Rect(0, 0, halfWidth, height));
      gfx::Canvas right = own.child(gfx::Rect(halfWidth, 0, width - halfWidth, height));
      gfx::drawCentred(left, reading.c_str(), face, scale);
      gfx::drawCentred(right, target.c_str(), face, scale);
      return;
    }

    gfx::drawCentred(own, reading.c_str(), face, scale);
    gfx::Canvas corner = own.child(gfx::Rect(halfWidth, halfHeight, width - halfWidth,
                                             height - halfHeight));
    gfx::drawCentred(corner, target.c_str(), face, secondaryScale);
  };
}

}  // namespace gardener::panel::page
#endif
