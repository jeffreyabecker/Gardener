#include "app/panel/DisconnectedUI.h"

#include "app/StreamLogger.h"
#include "app/VacuumSystem.h"
#include "app/panel/Asset.h"
#include "app/routines/Routines.h"
#include "core/Config.h"

namespace gardener {
namespace {

// How long OK has to be held before the page goes back to default. The same mark as the Back hold:
// long enough that a press cannot do it by accident.
constexpr uint32_t kDefaultHoldMs = 3000;

}  // namespace

DisconnectedUI::DisconnectedUI(AppContext context, StreamLogger& logger)
    : UserInterface(context), logger_(logger) {}

// A routine draws its own first frame, so the mark is only drawn when no routine is active. The
// page becoming visible is also when the routine re-reads what it knew — the unit may have changed
// while a controller owned the device.
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
  watchDefaultHold();

  if (context_.routines().active()) {
    context_.routines().poll(millis());
    return;
  }

  uint32_t now = millis();
  if (now - lastFrameMs_ < cfg::UI_REFRESH_MS) return;
  lastFrameMs_ = now;
  drawFrame();
}

// The page's own way out: holding OK past the mark leaves whatever routine is showing and puts
// `default` back on the panel. The machine is released first, because the page that was commanding it
// is about to be, and the default page should not open on a chamber still pulling. The latch keeps it
// to one switch per hold.
void DisconnectedUI::watchDefaultHold() {
  const uint32_t held = context_.buttons().heldMs(Button::Ok);
  if (held == 0) {
    defaultFired_ = false;
    return;
  }
  if (defaultFired_ || held < kDefaultHoldMs) return;
  defaultFired_ = true;

  logger_.warn("ui: ok held, stopping and back to the default page");
  context_.vacuum().stop();
  context_.routines().showDefault();
}

void DisconnectedUI::drawFrame() {
  Display& panel = context_.display();
  panel.clear();
  // Nothing when the image is not there: the page is blank rather than wrong, and the boot log is
  // where the missing file was named.
  drawAsset(panel, context_.storage(), kMarkAsset, 0, 0);
  panel.display();
}

}  // namespace gardener
