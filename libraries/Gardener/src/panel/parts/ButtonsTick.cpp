#include "Build.h"

#if GARDENER_PANEL

#include "panel/parts/ButtonsTick.h"

#include "core/Logger.h"
#include "machine/MachineCommand.h"
#include "panel/parts/GestureClassifier.h"
#include "routines/Routines.h"

namespace gardener::panel::parts {
namespace {

// The holds the panel acts on, as the marks the gesture carries in whole seconds: Okay and Cancel
// together past it restart the device, and Cancel alone past it force-quits the routine.
constexpr uint32_t ResetHoldSeconds = 5;

}  // namespace

ButtonsTick::ButtonsTick(GestureClassifier& gestures, Routines& routines, CommandOutbox& outbox,
                         Logger& logger)
    : gestures_(gestures), routines_(routines), outbox_(outbox), logger_(logger) {}

bool ButtonsTick::takeReset() {
  const bool pending = reset_;
  reset_ = false;
  return pending;
}

bool ButtonsTick::takeHome() {
  const bool pending = home_;
  home_ = false;
  return pending;
}

void ButtonsTick::tick() {
  const String gesture = gestures_.update();

  static const String ResetGesture =
      String("ok:") + ResetHoldSeconds + "+cancel:" + ResetHoldSeconds;
  if (gesture == ResetGesture) {
    logger_.warn("panel: reset held, rebooting");
    reset_ = true;
    return;
  }

  static const String QuitGesture = String("cancel:") + ResetHoldSeconds;
  // The quit is the panel's own safety gesture, so it does not wait for anything: Cancel held for
  // five seconds stops and vents. The stop is the device's, not the routine's — the routine is
  // being taken off the stack, so nothing of it runs to stop itself. The request tells the page to
  // show the root.
  if (gesture == QuitGesture) {
    logger_.warn("panel: cancel held, force-quitting");
    outbox_.post(MachineCommand{MachineCommand::Op::Stop});
    home_ = true;
    return;
  }

  if (!gesture.isEmpty()) deliver(gesture);
}

void ButtonsTick::deliver(const String& gesture) {
  if (routines_.active()) {
    routines_.pushPress(gesture);
    return;
  }
  String message = "btn: ";
  message += gesture;
  logger_.info(message.c_str());
}

}  // namespace gardener::panel::parts
#endif
