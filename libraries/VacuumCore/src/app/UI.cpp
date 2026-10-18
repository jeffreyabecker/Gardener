#include "UI.h"

#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#include "core/Config.h"
#include "eggplant-bitmap.h"

namespace gardener {

ConnectedUI::ConnectedUI(AppContext context) : UserInterface(context) {}

// The boot screen, and the only frame that is not a reading.
bool ConnectedUI::begin() {
  Display& panel = context_.display();
  panel.clear();
  panel.bitmap(0, 0, splashBitmap, panel.width(), panel.height());
  panel.display();
  startedMs_ = millis();
  return true;
}

// Repainted on a fixed cadence rather than when something changes: the battery and the state icon
// are drawn by Overlay on the way to every frame, and the battery's blink only happens while
// frames keep arriving.
void ConnectedUI::update() {
  uint32_t now = millis();
  if (now - startedMs_ < cfg::SPLASH_HOLD_MS) return;
  if (now - lastFrameMs_ < cfg::UI_REFRESH_MS) return;
  lastFrameMs_ = now;
  drawFrame();
}

void ConnectedUI::drawFrame() {
  Display& panel = context_.display();
  VacuumSystem& vacuum = context_.vacuum();

  // No valid reading is not a zero, and a target nobody set is not one either.
  String reading = vacuum.sensor().readingValid() ? String(vacuum.currentKpa(), 1) : String("--");
  String target = vacuum.targetKpa() > 0.0f ? String(vacuum.targetKpa(), 1) : String("--");

  panel.clear();

  panel.setFont(&FreeSansBold9pt7b);
  panel.text(0, 12, runStateName(vacuum.runState()), 1, false);

  panel.setFont(&FreeSansBold18pt7b);
  panel.text(0, 41, reading, 1, false);
  // textBounds measures in the face the panel is set to, so the unit is placed while the big face
  // is still the current one.
  int16_t width = 0;
  int16_t height = 0;
  panel.textBounds(reading, 1, width, height);
  panel.setFont(&FreeSansBold9pt7b);
  panel.text(width + 4, 41, "kPa", 1, false);

  panel.setFont(&FreeSansBold12pt7b);
  panel.text(0, 62, String("TGT ") + target, 1, false);

  panel.display();
}

}  // namespace gardener