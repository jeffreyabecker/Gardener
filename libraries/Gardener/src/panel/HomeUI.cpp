#include "Build.h"

#if GARDENER_PANEL

#include "panel/HomeUI.h"

#include "PanelTasks.h"
#include "panel/Mark.h"
#include "routines/Routines.h"

namespace gardener {

HomeUI::HomeUI(Panel& context) : UserInterface(context) {}

// A routine draws its own first frame, so the mark is only drawn when none is active. The first
// paint also activates the root scene, so it reads the unit and redraws rather than trusting what a
// previous boot left on the glass.
bool HomeUI::begin() {
  lastFrameMs_ = millis();
  lastGoHomeSeq_ = goHomeSeq;
  // The first paint also activates the root scene, so it reads the unit and redraws rather than
  // trusting what a previous boot left on the glass.
  if (context_.routines().enterPage()) return true;
  drawFrame();
  return true;
}

// The picture never changes, so this repaints on the readout's cadence rather than on every loop: a
// frame is a whole buffer over I2C. It still repeats, because the overlay's battery icon is drawn on
// the way to every frame and its low-charge blink only happens while frames keep arriving.
//
// With a routine active the page is the routine's: one entry into the VM per pass, and no frame of
// the mark behind it.
void HomeUI::update() {
  // The device's force-quit stops and vents from the panel task and bumps goHomeSeq; the page
  // goes back to `default` then.
  if (goHomeSeq != lastGoHomeSeq_) {
    lastGoHomeSeq_ = goHomeSeq;
    context_.routines().showDefault();
    return;
  }

  if (context_.routines().poll(millis())) return;

  uint32_t now = millis();
  if (now - lastFrameMs_ < RefreshMs) return;
  lastFrameMs_ = now;
  drawFrame();
}

void HomeUI::drawFrame() {
  Display& panel = context_.display();
  panel.clear(Paper);
  panel.bitmap(0, 0, MarkBits);
  panel.display();
}

}  // namespace gardener
#endif
