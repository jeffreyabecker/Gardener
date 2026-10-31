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

// A reading drawn at the place its cell gives it, at the face and scale the readout was given, with
// the ink it was measured at. The node keeps the text: it is built as the draw runs, so there is no
// page or literal behind it to borrow from.
gfx::PaintNodeRef readingNode(String value, const gfx::Font* face, uint8_t scale, gfx::Size ink,
                              gfx::Justify across) {
  return gfx::aligned(gfx::sized(gfx::text(gfx::Box::whole(), std::move(value), face, scale), ink),
                      across);
}

}  // namespace

gfx::PaintNodeRef pressureReadout(gfx::Box box, const SystemState& state, float sizeHint,
                                  const gfx::Font* face, uint8_t scale, uint8_t secondaryScale) {
  return gfx::callable([box, state, sizeHint, face, scale, secondaryScale](gfx::Canvas& canvas) {
    // A stopped machine has no chamber reading to show, so the reading is dashes; the target is
    // known either way.
    const String reading =
        state.running ? readingText(state.currentPressureKpa, state.unit) : String("--");
    const String target = readingText(state.targetPressureKpa, state.unit);

    // The two readings are two cells of one layout rather than two boxes over the same glass, which
    // is what keeps a reading as wide as the box from being drawn through the target: a cell is the
    // ink the reading takes, and no reading is drawn outside the cell it was given. Side by side
    // when both are at the main scale, and stacked — the target at the end of the lower cell —
    // when the target is the small one.
    const bool side = sizeHint >= 1.0f;
    const uint8_t targetScale = side ? scale : secondaryScale;
    const gfx::Size readingInk = gfx::measureText(face, reading.c_str(), scale);
    const gfx::Size targetInk = gfx::measureText(face, target.c_str(), targetScale);

    gfx::PaintNodeRef large =
        readingNode(String(reading), face, scale, readingInk, gfx::Justify::Center);
    gfx::PaintNodeRef small = readingNode(String(target), face, targetScale, targetInk,
                                          side ? gfx::Justify::Center : gfx::Justify::End);

    gfx::PaintNodeRef readout;
    if (side) {
      readout = gfx::row(1.0f, gfx::children(gfx::Child{gfx::Cell(0.5f, 1.0f), std::move(large)},
                                             gfx::Child{gfx::Cell(0.5f, 1.0f), std::move(small)}));
    } else {
      readout = gfx::column(
          1.0f,
          gfx::children(
              gfx::Child{gfx::Cell(1.0f, static_cast<float>(readingInk.height)), std::move(large)},
              gfx::Child{gfx::Cell(1.0f, static_cast<float>(targetInk.height)), std::move(small)}),
          gfx::Justification{gfx::Justify::Center, gfx::Justify::Start});
    }
    gfx::View own = canvas.child(box.in(canvas));
    readout->draw(own);
  });
}

}  // namespace gardener