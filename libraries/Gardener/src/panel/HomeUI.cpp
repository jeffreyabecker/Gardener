#include "Build.h"

#if GARDENER_PANEL

#include "panel/HomeUI.h"

#include "PanelTasks.h"
#include "routines/Routines.h"

namespace gardener {

HomeUI::HomeUI(Panel& context) : UserInterface(context) {}

// The root scene goes on with the first page, so a scene re-reads the unit and draws rather than
// trusting what a previous boot left on the glass.
bool HomeUI::begin() {
  lastGoHomeSeq_ = goHomeSeq;
  context_.routines().enterPage();
  return true;
}

// The beat belongs to the stack: it polls the top scene, draws the band over it and sends the frame,
// on its own repaint cadence.
void HomeUI::update() {
  // The device's force-quit stops and vents from the panel task and bumps goHomeSeq; the page goes
  // back to `default` then.
  if (goHomeSeq != lastGoHomeSeq_) {
    lastGoHomeSeq_ = goHomeSeq;
    context_.routines().showDefault();
    return;
  }

  context_.routines().poll(millis());
}

}  // namespace gardener
#endif
