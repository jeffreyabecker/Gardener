#include "app/DisconnectedUI.h"

#include "app/eggplant-bitmap.h"
#include "core/Config.h"

namespace gardener {

DisconnectedUI::DisconnectedUI(AppContext context) : UserInterface(context) {}

// Nothing to set up: the frame is the whole screen.
bool DisconnectedUI::begin() {
  drawFrame();
  lastFrameMs_ = millis();
  return true;
}

// The picture never changes, so this repaints on the readout's cadence rather than on every loop: a
// frame is a whole buffer over I2C. It still repeats, because the overlay's battery icon is drawn on
// the way to every frame and its low-charge blink only happens while frames keep arriving.
void DisconnectedUI::update() {
  uint32_t now = millis();
  if (now - lastFrameMs_ < cfg::UI_REFRESH_MS) return;
  lastFrameMs_ = now;
  drawFrame();
}

void DisconnectedUI::drawFrame() {
  Display& panel = context_.display();
  panel.clear();
  panel.bitmap(0, 0, splashBitmap, panel.width(), panel.height());
  panel.display();
}

}  // namespace gardener
