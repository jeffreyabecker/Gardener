#include "beats/ButtonsTick.h"

#include "core/Gestures.h"
#include "core/Knob.h"
#include "core/Logger.h"
#include "machine/CommandQueue.h"
#include "machine/MachineCommand.h"
#include "panel/GestureQueue.h"

namespace gardener {
namespace {

// The holds the panel acts on, as the marks the gesture carries in whole seconds: Okay and Cancel
// together past it restart the device, and Cancel alone past it force-quits the routine.
GARDENER_KNOB_VALID(uint32_t, ResetHoldSeconds, GARDENER_BUTTON_RESET_HOLD_SECONDS, 5,
                    GARDENER_BUTTON_RESET_HOLD_SECONDS >= 1 &&
                        GARDENER_BUTTON_RESET_HOLD_SECONDS <= 60,
                    "the hold that restarts the device, in whole seconds");

}  // namespace

ButtonsTick::ButtonsTick(ButtonPanel& buttons, GestureQueue& presses, CommandQueue& queue,
                         Logger& logger)
    : classifier_(buttons), presses_(presses), queue_(queue), logger_(logger) {}

void ButtonsTick::begin() {
  // The classifier answers the seconds a key is held; the panel's two safety holds are named as the
  // commands they are, so what it answers for them is a word rather than a count.
  classifier_.addOverride(String("ok:") + ResetHoldSeconds + "+cancel:" + ResetHoldSeconds,
                          Gestures::SystemReboot);
  classifier_.addOverride(String("cancel:") + ResetHoldSeconds, Gestures::SystemQuit);
}

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

  // The reset and the quit arrive under the names begin() gave them.
  if (Gestures::is(gesture.c_str(), Gestures::SystemReboot)) {
    logger_.warn("panel: reset held, rebooting");
    reset_ = true;
    return;
  }

  // The quit is the panel's own safety gesture, so it does not wait for anything: Cancel held for
  // five seconds stops and vents. The stop is the device's, not the routine's — the routine is
  // being taken off the stack, so nothing of it runs to stop itself. The request tells the page to
  // show the root.
  if (Gestures::is(gesture.c_str(), Gestures::SystemQuit)) {
    logger_.warn("panel: cancel held, force-quitting");
    queue_.post(MachineCommand{MachineCommand::Op::Stop});
    home_ = true;
    return;
  }

  if (!gesture.isEmpty()) deliver(gesture.c_str());
}

void ButtonsTick::deliver(const char* gesture) { presses_.post(gesture); }

}  // namespace gardener