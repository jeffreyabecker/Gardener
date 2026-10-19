#include "app/DisconnectedUI.h"

#include "app/eggplant-bitmap.h"
#include "app/disconnected-ui/Routines.h"
#include "core/Config.h"

namespace gardener {

DisconnectedUI::DisconnectedUI(AppContext context) : UserInterface(context) {}

// A routine draws its own first frame, so the mark is only drawn when no routine is active. The
// page becoming visible is also when the routine re-reads what it knew — the unit may have changed
// while a host was attached.
bool DisconnectedUI::begin() {
  lastFrameMs_ = millis();
  if (context_.routines().active()) {
    context_.routines().enterPage();
    return true;
  }
  drawFrame();
  return true;
}

// The picture never changes, so this repaints on the readout's cadence rather than on every loop: a
// frame is a whole buffer over I2C. It still repeats, because the overlay's battery icon is drawn on
// the way to every frame and its low-charge blink only happens while frames keep arriving.
//
// With a routine active the page is the routine's: one entry into the VM per pass, and no frame of
// the mark behind it.
void DisconnectedUI::update() {
  if (context_.routines().active()) {
    context_.routines().poll(millis());
    return;
  }

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
