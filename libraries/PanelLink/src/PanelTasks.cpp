#include "PanelTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Logger.h"
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

// The panel's holds, which are the marks the gesture carries in whole seconds. A Back held past
// the stop mark releases the chamber; Back and Okay together past the reset mark restart the device.
constexpr uint32_t BackStopSeconds = 3;
constexpr uint32_t ResetHoldSeconds = 5;

// The panel task's own state: the two screens it owns, the one showing, and the one latch the Back
// hold needs.
struct PanelState {
  Panel& context;
  Logger* logger;
  ConnectedUI* connected;
  DisconnectedUI* disconnected;
  UserInterface* ui = nullptr;
  bool ownedShown = false;
  // One stopSeq bump per Back hold, so a page sees the stop once rather than every beat.
  bool backStopFired = false;
};

// The whole seconds a gesture's Back part carries, or 0 when it has none. Back is first in panel
// order, so its part is the gesture's first.
uint32_t backSeconds(const String& gesture) {
  if (!gesture.startsWith("back:")) return 0;
  int end = gesture.indexOf('+');
  if (end < 0) end = gesture.length();
  return (uint32_t)gesture.substring(5, end).toInt();
}

// The gesture goes to the active routine on the panel's own pages, or to the press log. The
// classifier is its one consumer, so it is never delivered twice.
void deliverGesture(PanelState& state, const String& gesture);

// One call to the classifier, and the policy for what it answers. The device's own gestures come
// first and are never delivered: the reset reboots, the Back hold stops and vents, and Cancel
// during that hold is the way home.
void buttonsTick(PanelState& state) {
  const String gesture = state.context.gestures().update();

  static const String ResetGesture =
      String("back:") + ResetHoldSeconds + "+ok:" + ResetHoldSeconds;
  if (gesture == ResetGesture) {
    state.logger->warn("panel: reset held, rebooting");
    safeReboot(state.context);
    return;
  }

  const uint32_t back = backSeconds(gesture);
  if (back < BackStopSeconds) state.backStopFired = false;
  if (back >= BackStopSeconds) {
    // The stop is re-asserted each beat the hold lasts, so a start that sneaks in is undone in the
    // same beat, and only while the panel drives.
    const bool owned = state.context.ownership().ownedBy(state.context);
    if (!state.backStopFired) {
      state.backStopFired = true;
      state.logger->warn("panel: back held, stopping");
      stopSeq++;
    }
    if (owned) {
      const MachineCommand stop{MachineCommand::Op::Stop};
      postMachineCommand(stop);
    }
    // Cancel beside the hold is the way home, and it is the panel's gesture rather than a page's.
    if (gesture.endsWith("+cancel:short")) {
      if (owned) goHomeSeq++;
    }
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
