#include "Build.h"

#if GARDENER_PANEL

#include "tasks/PanelTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/Logger.h"
#include "machine/MachineCommand.h"
#include "panel/Panel.h"
#include "machine/SafeReboot.h"
#include "tasks/MachineTasks.h"
#include "panel/HomeUI.h"

namespace gardener {

namespace {

// The panel task runs lowest because a routine's poll may take its time, and its stack is the one
// the Berry interpreter runs inside.
constexpr UBaseType_t PanelPriority = 0;
constexpr uint32_t PanelStackWords = 8192;
constexpr UBaseType_t PanelCore = 0;

// The holds the panel task acts on, as the marks the gesture carries in whole seconds: Okay and
// Cancel together past it restart the device, and Cancel alone past it force-quits the routine.
constexpr uint32_t ResetHoldSeconds = 5;

// The panel task's own state: the machine it drives and the one page it shows.
struct PanelState {
  Panel& context;
  Logger* logger;
  HomeUI* home;
};

// The gesture goes to the active routine on the panel page, or to the press log. The classifier is
// its one consumer, so it is never delivered twice.
void deliverGesture(PanelState& state, const String& gesture);

// One call to the classifier, and the policy for what it answers. The reboot and the force-quit are
// the gestures the panel task takes for itself; everything else is a page's.
void buttonsTick(PanelState& state) {
  const String gesture = state.context.gestures().update();

  static const String ResetGesture =
      String("ok:") + ResetHoldSeconds + "+cancel:" + ResetHoldSeconds;
  if (gesture == ResetGesture) {
    state.logger->warn("panel: reset held, rebooting");
    safeReboot(state.context);
    return;
  }

  static const String QuitGesture = String("cancel:") + ResetHoldSeconds;
  // The quit is the panel's own safety gesture, so it does not wait for anything: Cancel held for
  // five seconds stops and vents. The stop is the device's, not the routine's — the routine is
  // being taken off the stack, so nothing of it runs to stop itself. The request tells the page to
  // show the root.
  if (gesture == QuitGesture) {
    state.logger->warn("panel: cancel held, force-quitting");
    state.context.post(MachineCommand{MachineCommand::Op::Stop});
    state.context.requestHome();
    return;
  }

  if (!gesture.isEmpty()) deliverGesture(state, gesture);
}

void deliverGesture(PanelState& state, const String& gesture) {
  if (state.context.routines().active()) {
    state.context.routines().pushPress(gesture);
    return;
  }
  String message = "btn: ";
  message += gesture;
  state.logger->info(message.c_str());
}

}  // namespace

void panelTask(void* arg) {
  PanelState& state = *static_cast<PanelState*>(arg);

  if (state.home) state.home->begin();

  for (;;) {
    // A command the panel could not hand to the control task on its last beat is retried first.
    state.context.flushCommands();
    buttonsTick(state);

    // The panel's own beat: the gauge and the buzzer, then the glass.
    const uint32_t now = millis();
    state.context.battery().update(now);
    state.context.buzzer().update(now);

    if (state.home) state.home->update();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void spawnPanelTask(Panel& context, HomeUI* home) {
  static PanelState panel{context, &context.logger(), home};

  xTaskCreatePinnedToCore(panelTask, "panel", PanelStackWords, &panel, PanelPriority, nullptr,
                          PanelCore);
}

}  // namespace gardener
#endif
