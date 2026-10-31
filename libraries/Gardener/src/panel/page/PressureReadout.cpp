#include "panel/page/PressureReadout.h"

#include <Arduino.h>
#include <stdio.h>

#include "shapes/Text.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener::panel::page {
namespace {

// A reading as the panel prints it: the unit's scale, and the decimals the unit's fine step can
// express.
String readingText(float pressureKpa, DisplayUnit unit) {
  char out[16];
  snprintf(out, sizeof(out), "%.*f", static_cast<int>(displayDecimals(unit)),
           displayValue(pressureKpa, unit));
  return out;
}

// A reading centred in the space it is handed, at the face and scale the readout was given.
gfx::PaintNodeRef centredReading(const String& value, const gfx::Font* face, uint8_t scale) {
  const gfx::Size ink = gfx::shapes::measureText(face, value.c_str(), scale);
  return gfx::aligned(gfx::sized(
      gfx::borrowedText(gfx::Box::whole(), value.c_str(), face, scale), ink));
}

}  // namespace

gfx::PaintNodeRef pressureReadout(gfx::Box box, const SystemState& state, float sizeHint,
                                  const gfx::Font* face, uint8_t scale, uint8_t secondaryScale) {
  return gfx::callable([box, state, sizeHint, face, scale, secondaryScale](gfx::Canvas& canvas) {
    // A stopped machine has no chamber reading to show, so the reading is dashes; the target is
    // known either way.
    const String reading = state.running
                               ? readingText(state.currentPressureKpa, state.unit)
                               : String("--");
    const String target = readingText(state.targetPressureKpa, state.unit);

    gfx::View own = canvas.child(box.in(canvas));
    const int16_t width = own.width();
    const int16_t height = own.height();
    const int16_t halfWidth = width / 2;
    const int16_t halfHeight = height / 2;

    if (sizeHint >= 1.0f) {
      gfx::View left = own.child(gfx::Rect(0, 0, halfWidth, height));
      gfx::View right = own.child(gfx::Rect(halfWidth, 0, width - halfWidth, height));
      centredReading(reading, face, scale)->draw(left);
      centredReading(target, face, scale)->draw(right);
      return;
    }

    centredReading(reading, face, scale)->draw(own);
    gfx::View corner = own.child(gfx::Rect(halfWidth, halfHeight, width - halfWidth,
                                             height - halfHeight));
    centredReading(target, face, secondaryScale)->draw(corner);
  });
}

}  // namespace gardener::panel::page
