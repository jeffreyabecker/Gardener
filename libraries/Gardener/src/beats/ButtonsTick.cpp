#include "beats/ButtonsTick.h"

#include "core/Logger.h"
#include "machine/MachineCommand.h"
#include "panel/GestureQueue.h"

namespace gardener {
inline namespace beats {
namespace {

// The holds the panel acts on, as the marks the gesture carries in whole seconds: Okay and Cancel
// together past it restart the device, and Cancel alone past it force-quits the routine.
constexpr uint32_t ResetHoldSeconds = 5;

}  // namespace

ButtonsTick::ButtonsTick(ButtonPanel& buttons, panel::GestureQueue& presses,
                         CommandOutbox& outbox, Logger& logger)
    : classifier_(buttons), presses_(presses), outbox_(outbox), logger_(logger) {}

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
  const String gesture = classifier_.update();

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

void ButtonsTick::deliver(const String& gesture) { presses_.push(gesture); }

}  // namespace beats
}  // namespace gardener
