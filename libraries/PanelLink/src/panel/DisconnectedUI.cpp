#include "panel/DisconnectedUI.h"

#include "PanelTasks.h"
#include "panel/Asset.h"
#include "routines/Routines.h"
#include "core/Config.h"

namespace gardener {

DisconnectedUI::DisconnectedUI(PanelContext& context) : UserInterface(context) {}

// A routine draws its own first frame, so the mark is only drawn when no routine is active. The
// page becoming visible is also when the routine re-reads what it knew — the unit may have changed
// while a controller owned the device — and when a go-home that fired on the other page is forgotten.
bool DisconnectedUI::begin() {
  lastFrameMs_ = millis();
  lastGoHomeSeq_ = goHomeSeq;
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
  // The device's Back hold stops and vents from the panel task and keeps the valve open while
  // Back is down; an OK pressed during that hold is the page's way out, so the panel goes back to
  // `default` then. The page being left does not get its stopped() notice: the switch drops it.
  if (goHomeSeq != lastGoHomeSeq_) {
    lastGoHomeSeq_ = goHomeSeq;
    context_.routines().showDefault();
    return;
  }

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
  // Nothing when the image is not there: the page is blank rather than wrong, and the boot log is
  // where the missing file was named.
  drawAsset(panel, kMarkAsset, 0, 0);
  panel.display();
}

}  // namespace gardener
