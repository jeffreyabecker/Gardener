#include "app/ConnectedUI.h"

#include "app/Asset.h"
#include "app/Fonts.h"
#include "core/Config.h"
#include "core/Units.h"

namespace gardener {

ConnectedUI::ConnectedUI(AppContext context) : UserInterface(context) {}

// The boot screen, and the only frame that is not a reading.
bool ConnectedUI::begin() {
  Display& panel = context_.display();
  panel.clear();
  drawAsset(panel, context_.storage(), kMarkAsset, 0, 0);
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
  // The machine holds kPa; the unit is the panel's, and so are the decimals it can express.
  const DisplayUnit unit = context_.settings().unit();
  const unsigned int decimals = displayDecimals(unit);

  // No valid reading is not a zero, and a target nobody set is not one either.
  String reading = vacuum.sensor().readingValid()
                       ? String(displayValue(vacuum.currentKpa(), unit), decimals)
                       : String("--");
  String target = vacuum.targetKpa() > 0.0f
                      ? String(displayValue(vacuum.targetKpa(), unit), decimals)
                      : String("--");

  panel.clear();

  panel.setFont(fonts::SansBold9);
  panel.text(0, 12, runStateName(vacuum.runState()), 1, false);

  panel.setFont(fonts::SansBold18);
  panel.text(0, 41, reading, 1, false);
  // textBounds measures in the face the panel is set to, so the unit is placed while the big face
  // is still the current one.
  int16_t width = 0;
  int16_t height = 0;
  panel.textBounds(reading, 1, width, height);
  panel.setFont(fonts::SansBold9);
  panel.text(width + 4, 41, displayUnitInfo(unit).suffix, 1, false);

  panel.setFont(fonts::SansBold12);
  panel.text(0, 62, String("TGT ") + target, 1, false);

  panel.display();
}

}  // namespace gardener