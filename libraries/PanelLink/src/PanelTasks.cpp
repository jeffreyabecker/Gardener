#include "PanelTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Logger.h"
#include "MachineCommand.h"
#include "Ownership.h"
#include "Panel.h"
#include "SafeReboot.h"
#include "Tasks.h"
#include "panel/ConnectedUI.h"
#include "panel/DisconnectedUI.h"
#include "panel/UI.h"

namespace gardener {

volatile uint32_t stopSeq = 0;
volatile uint32_t goHomeSeq = 0;

namespace {

// The panel task runs lowest because a routine's poll may take its time, and its stack is the one
// the Berry interpreter runs inside.
constexpr UBaseType_t PanelPriority = 0;
constexpr uint32_t PanelStackWords = 8192;
constexpr UBaseType_t PanelCore = 0;

// The holds the panel task acts on, as the marks the gesture carries in whole seconds: Okay and
// Cancel together past it restart the device, and Cancel alone past it force-quits the routine.
constexpr uint32_t ResetHoldSeconds = 5;

// The panel task's own state: the two screens it owns and the one showing.
struct PanelState {
  Panel& context;
  Logger* logger;
  ConnectedUI* connected;
  DisconnectedUI* disconnected;
  UserInterface* ui = nullptr;
  bool ownedShown = false;
};

// The gesture goes to the active routine on the panel's own pages, or to the press log. The
// classifier is its one consumer, so it is never delivered twice.
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
  // The quit is the panel's own safety gesture, so it does not wait for control: whoever drives,
  // Cancel held for five seconds stops and vents. The stop is the device's, not the routine's —
  // the routine is being taken off the stack, so nothing of it runs to stop itself. goHomeSeq
  // tells the disconnected page to show the root.
  if (gesture == QuitGesture) {
    state.logger->warn("panel: cancel held, force-quitting");
    state.context.post(MachineCommand{MachineCommand::Op::Stop});
    goHomeSeq++;
    return;
  }

  if (!gesture.isEmpty()) deliverGesture(state, gesture);
}

void deliverGesture(PanelState& state, const String& gesture) {
  if (state.context.ownership().ownedBy(state.context) && state.context.routines().active()) {
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

  // A fresh boot starts with the panel on top: an external link that is present is only a reader
  // until it claims, so the panel's own page shows first.
  state.ownedShown = state.context.externalPage();
  state.ui = state.ownedShown ? static_cast<UserInterface*>(state.connected)
                              : static_cast<UserInterface*>(state.disconnected);
  if (state.ui) state.ui->begin();

  for (;;) {
    // A command the panel could not hand to the control task on its last beat is retried first.
    state.context.flushCommands();
    buttonsTick(state);

    // The screen follows control, not the cable: an external claim paints the link's page, and a
    // release hands the panel back. The hand-off vents the run, so the page that returns sees the
    // stop on its first poll.
    const bool external = state.context.externalPage();
    if (external != state.ownedShown) {
      state.ownedShown = external;
      stopSeq++;
      state.ui = external ? static_cast<UserInterface*>(state.connected)
                          : static_cast<UserInterface*>(state.disconnected);
      if (state.ui) state.ui->begin();
    }

    // The panel's own beat: the gauge and the buzzer, then the glass.
    const uint32_t now = millis();
    state.context.battery().update(now);
    state.context.buzzer().update(now);

    // The overlay reports the ways in; the interface owner publishes the two booleans, and the
    // pages redraw themselves on the refresh cadence.
    state.context.overlay().setConnections(serialConnected, bleConnected);
    if (state.ui) state.ui->update();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void spawnPanelTask(Panel& context, ConnectedUI* connected,
                    DisconnectedUI* disconnected) {
  static PanelState panel{context, &context.logger(), connected, disconnected};

  xTaskCreatePinnedToCore(panelTask, "panel", PanelStackWords, &panel, PanelPriority, nullptr,
                          PanelCore);
}

}  // namespace gardener
