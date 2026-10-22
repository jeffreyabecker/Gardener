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
#include "hal/Button.h"

namespace gardener {

volatile uint32_t stopSeq = 0;
volatile uint32_t goHomeSeq = 0;

namespace {

// The panel task runs lowest because a routine's poll may take its time, and its stack is the one
// the Berry interpreter runs inside.
constexpr UBaseType_t PanelPriority = 0;
constexpr uint32_t PanelStackWords = 8192;
constexpr UBaseType_t PanelCore = 0;

// The keys in the order a press is reported, which is the panel's order rather than the bit order.
constexpr Button PressOrder[] = {Button::Up, Button::Down, Button::Ok, Button::Back};

// The panel's holds. Holding Back for this long stops the run and vents; Back+OK together for the
// longer mark is the reboot.
constexpr uint32_t BackStopMs = 3000;
constexpr uint32_t ResetHoldMs = 5000;

// The panel task's own state: the two screens it owns, the one showing, which page it is, and the
// chord and hold latches the keys need.
struct PanelState {
  Panel& context;
  Logger* logger;
  ConnectedUI* connected;
  DisconnectedUI* disconnected;
  UserInterface* ui = nullptr;
  bool ownedShown = false;
  // The Up+Down chord: both keys were down at once, and the chord event has gone out. The pair's
  // own releases are not page presses while either flag is set.
  bool upDownChord = false;
  bool upDownChordFired = false;
  // One stopSeq bump per Back hold, so a page sees the stop once rather than every beat.
  bool backStopFired = false;
};

// A classified press goes to the active routine on the panel's own pages, or to the press log. A
// press is consumed either way, so it is never delivered twice.
void deliverPress(PanelState& state, Button button, Press press);

// The keys, settled and classified on the panel beat: the button driver is single-consumer (its
// settle() mutates one state machine), and the panel is its one consumer now that the control task
// reads no keys. One beat classifies the presses, watches the two holds, and feeds whichever page
// owns them. OK while the Back hold vents is not a page action: on the panel's own pages it is the
// way home, so it is stolen rather than delivered.
//
// Up and Down together are one chord, not two presses: the pair is swallowed and a single chord
// event is delivered on the up key when the first of the two is released.
void buttonsTick(PanelState& state) {
  const bool upDown = state.context.buttons().pressed(Button::Up);
  const bool downDown = state.context.buttons().pressed(Button::Down);
  const bool bothDown = upDown && downDown;

  // The two holds are the panel's gestures. Back+OK is the reboot, whatever owns the machine; Back
  // is the stop, so it fires only while the panel drives.
  if (state.context.buttons().heldMs(Button::Back) >= ResetHoldMs &&
      state.context.buttons().heldMs(Button::Ok) >= ResetHoldMs) {
    state.logger->warn("panel: reset held, rebooting");
    safeReboot(state.context);
    return;
  }
  const bool backHeld = state.context.buttons().heldMs(Button::Back) >= BackStopMs;
  if (backHeld && state.context.ownership().ownedBy(state.context)) {
    // One bump per hold, and the stop re-asserted each beat the hold lasts, so a start that sneaks
    // in is undone in the same beat.
    if (!state.backStopFired) {
      state.backStopFired = true;
      state.logger->warn("panel: back held, stopping");
      stopSeq++;
    }
    const MachineCommand stop{MachineCommand::Op::Stop};
    postMachineCommand(stop);
  }
  if (!backHeld) state.backStopFired = false;

  for (Button button : PressOrder) {
    // The chord's keys are one gesture, so their presses are dropped while the pair is held or has
    // just fired. The drop reaches into the double window too, so the releases never arrive late.
    if ((state.upDownChord || state.upDownChordFired) &&
        (button == Button::Up || button == Button::Down)) {
      state.context.buttons().dropPress(button);
      continue;
    }
    const Press press = state.context.buttons().consumePress(button);
    if (press == Press::None) continue;
    if (backHeld && state.context.ownership().ownedBy(state.context) && button == Button::Ok) {
      goHomeSeq++;
      continue;
    }
    deliverPress(state, button, press);
  }

  // Advance the chord: both down arms it, the first release fires it, and both up clears it.
  if (bothDown && !state.upDownChordFired) {
    state.upDownChord = true;
  } else if (state.upDownChord && !state.upDownChordFired && !bothDown) {
    state.upDownChordFired = true;
    deliverPress(state, Button::Up, Press::Chord);
  }
  if (!upDown && !downDown) {
    state.upDownChord = false;
    state.upDownChordFired = false;
  }
}

void deliverPress(PanelState& state, Button button, Press press) {
  if (state.context.ownership().ownedBy(state.context) && state.context.routines().active()) {
    state.context.routines().pushPress(button, press);
    return;
  }
  String message = "btn: ";
  message += buttonName(button);
  message += ' ';
  message += pressName(press);
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
