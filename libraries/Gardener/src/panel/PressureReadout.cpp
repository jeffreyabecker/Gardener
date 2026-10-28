#include "Build.h"

#if GARDENER_PANEL

#include "panel/PressureReadout.h"

#include <stdio.h>
#include <string.h>

#include <string>

namespace gardener {
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

gfx::ui::Paint pressureReadout(gfx::Rect box, VacuumSystem& vacuum, DisplayUnit unit,
                               ReadoutOrder order, const char* position, float sizeHint,
                               const gfx::Font* face, uint8_t scale, uint8_t secondaryScale) {
  // The side comes from the last character of the position and the row from the first, so `tr` is
  // the top right and anything unrecognised the bottom right.
  const char* corner = position == nullptr ? "br" : position;
  const size_t length = strlen(corner);
  const bool secondaryLeft = length > 0 && corner[length - 1] == 'l';
  const bool secondaryTop = length > 0 && corner[0] == 't';

  return [box, &vacuum, unit, order, secondaryLeft, secondaryTop, sizeHint, face, scale,
          secondaryScale](gfx::Canvas& canvas) {
    const float pressure = vacuum.currentPressureKpa();
    const float target = vacuum.targetPressureKpa();
    const bool targetFirst = order == ReadoutOrder::TargetPressure;
    // A stopped machine has no chamber reading to show, so the pressure number is dashes; the
    // target is known either way.
    const std::string pressureText =
        vacuum.running() ? readingText(pressure, unit) : std::string("--");
    const std::string targetText = readingText(target, unit);
    const std::string main = targetFirst ? targetText : pressureText;
    const std::string second = targetFirst ? pressureText : targetText;

    gfx::Canvas own = canvas.child(box);
    const int16_t width = own.width();
    const int16_t height = own.height();
    const int16_t halfWidth = width / 2;
    const int16_t halfHeight = height / 2;

    if (sizeHint >= 1.0f) {
      gfx::Canvas left = own.child(gfx::Rect(0, 0, halfWidth, height));
      gfx::Canvas right = own.child(gfx::Rect(halfWidth, 0, width - halfWidth, height));
      gfx::ui::drawCentred(left, secondaryLeft ? second.c_str() : main.c_str(), face, scale);
      gfx::ui::drawCentred(right, secondaryLeft ? main.c_str() : second.c_str(), face, scale);
      return;
    }

    gfx::ui::drawCentred(own, main.c_str(), face, scale);
    gfx::Canvas cell =
        own.child(gfx::Rect(secondaryLeft ? 0 : halfWidth, secondaryTop ? 0 : halfHeight,
                            secondaryLeft ? halfWidth : width - halfWidth,
                            secondaryTop ? halfHeight : height - halfHeight));
    gfx::ui::drawCentred(cell, second.c_str(), face, secondaryScale);
  };
}

}  // namespace gardener
#endif
