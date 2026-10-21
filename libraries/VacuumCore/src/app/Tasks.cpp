#include "app/Tasks.h"

#include "app/Logger.h"
#include "app/MachineContext.h"
#include "app/Ownership.h"
#include "app/PanelContext.h"
#include "app/SafeReboot.h"
#include "app/StoreBus.h"
#include "app/VacuumSystem.h"
#include "app/ble/BleLink.h"
#include "app/panel/ConnectedUI.h"
#include "app/panel/DisconnectedUI.h"
#include "app/panel/UI.h"
#include "app/serial/SerialLink.h"
#include "core/Config.h"

namespace gardener {

QueueHandle_t machineCommandQueue = nullptr;
QueueHandle_t storeQueue = nullptr;
volatile uint32_t stopSeq = 0;
volatile uint32_t goHomeSeq = 0;
volatile bool serialConnected = false;
volatile bool bleConnected = false;

namespace {

// Task configuration. The control task runs above the
// Arduino loop's default priority so the top-change vent and the PID are never preempted by a link
// or the screen; the panel task runs lowest because a routine's poll may take its time. The panel's
// stack is the one the Berry interpreter runs inside.
constexpr UBaseType_t ControlPriority = 2;
constexpr UBaseType_t InterfacePriority = 1;
constexpr UBaseType_t PanelPriority = 0;
constexpr uint32_t ControlStackWords = 4096;
constexpr uint32_t InterfaceStackWords = 4096;
constexpr uint32_t PanelStackWords = 8192;
constexpr UBaseType_t ControlCore = 1;
constexpr UBaseType_t InterfaceCore = 0;
constexpr UBaseType_t PanelCore = 0;

// Queue depths. Overflow drops the newest item with a log
// line; the consumers drain every tick, so a full queue is the exception, not the steady state.
constexpr UBaseType_t MachineCommandQueueDepth = 8;

// The keys in the order a press is reported, which is the panel's order rather than the bit order.
constexpr Button PressOrder[] = {Button::Up, Button::Down, Button::Ok, Button::Back};

// The panel's holds. Holding Back for this long stops the run and vents; Back+OK together for the
// longer mark is the reboot.
constexpr uint32_t BackStopMs = 3000;
constexpr uint32_t ResetHoldMs = 5000;

// The control task's own state: the machine it commands, the logger it reports through, and the
// latch/timing fields the run log and the top-change vent need.
struct ControlState {
  MachineContext& context;
  Logger* logger;
  RunState lastState = RunState::Idle;
  uint32_t ventStartMs = 0;
  Link lastOwner = Link::None;
};

// The panel task's own state: the two screens it owns, the one showing, which page it is, and the
// chord and hold latches the keys need.
struct PanelState {
  PanelContext& context;
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

// The interface task's own state: the machine it reports through, the links, and the logger.
struct InterfaceState {
  MachineContext& context;
  Logger* logger;
  SerialLink* link;
  BleLink* ble;
};

// Applies one queued write to the machine. The op names the verb; the fields are the op's own.
void applyMachineCommand(ControlState& state, const MachineCommand& command) {
  MachineContext& context = state.context;
  switch (command.op) {
    case MachineCommand::Op::Start: context.vacuum().start(); break;
    case MachineCommand::Op::Stop: context.vacuum().stop(); break;
    case MachineCommand::Op::ResetFault: context.vacuum().resetFault(); break;
    case MachineCommand::Op::SetTarget: context.vacuum().setTarget(command.value); break;
    case MachineCommand::Op::ClearTarget: context.vacuum().clearTarget(); break;
    case MachineCommand::Op::Reboot: safeReboot(context); break;
  }
}

// Every pending command, applied against the machine as the last tick left it.
void drainMachineCommands(ControlState& state) {
  MachineCommand command;
  while (xQueueReceive(machineCommandQueue, &command, 0) == pdTRUE) applyMachineCommand(state, command);
}

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
    safeReboot(state.context.machine());
    return;
  }
  const bool backHeld = state.context.buttons().heldMs(Button::Back) >= BackStopMs;
  if (backHeld && state.context.machine().ownership().ownedBy(Link::Panel)) {
    // One bump per hold, and the stop re-asserted each beat the hold lasts, so a start that sneaks
    // in is undone in the same beat.
    if (!state.backStopFired) {
      state.backStopFired = true;
      state.logger->warn("panel: back held, stopping");
      stopSeq++;
    }
    const MachineCommand stop{MachineCommand::Op::Stop};
    xQueueSend(machineCommandQueue, &stop, 0);
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
    if (backHeld && !state.context.machine().ownership().external() && button == Button::Ok) {
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
  if (!state.context.machine().ownership().external() && state.context.routines().active()) {
    state.context.routines().pushPress(button, press);
    return;
  }
  String message = "btn: ";
  message += buttonName(button);
  message += ' ';
  message += pressName(press);
  state.logger->info(message.c_str());
}

// The drivers stopped narrating, so the run is reported from here: one line per state change,
// timed when it leaves a vent, plus the fault code when one latches.
void logRunChanges(ControlState& state, uint32_t nowMs) {
  const RunState current = state.context.vacuum().runState();
  if (current == state.lastState) return;

  String message = runStateName(state.lastState);
  message += " -> ";
  message += runStateName(current);
  if (current == RunState::Fault) {
    message += " (fault ";
    message += state.context.vacuum().faultCode();
    message += ')';
  }
  if (state.lastState == RunState::Venting) {
    const uint32_t ventMs = nowMs - state.ventStartMs;
    message += " after ";
    message += String(ventMs);
    message += " ms";
    if (ventMs >= cfg::VENT_TIMEOUT_MS) {
      message += ", vent timed out";
      state.logger->warn(message.c_str());
      state.lastState = current;
      return;
    }
  }
  if (current == RunState::Venting) state.ventStartMs = nowMs;
  state.logger->info(message.c_str());
  state.lastState = current;
}

}  // namespace

void controlTask(void* arg) {
  ControlState& state = *static_cast<ControlState*>(arg);
  state.lastState = state.context.vacuum().runState();
  state.lastOwner = state.context.ownership().owner();

  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    const uint32_t now = millis();

    // Commands first, against the machine as the last tick left it.
    drainMachineCommands(state);

    // The top of the stack changed — a claim, a release or a drop all vent — so a new controller
    // starts from an open chamber.
    const Link top = state.context.ownership().owner();
    if (top != state.lastOwner) {
      state.lastOwner = top;
      state.context.vacuum().stop();
      String changed = "app: control to ";
      changed += linkName(top);
      state.logger->info(changed.c_str());
    }

    // The devices and the PID on the fixed beat, then the run log.
    state.context.sensor().update(now);
    state.context.pump().update(now);
    state.context.vacuum().service(now);
    logRunChanges(state, now);

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(cfg::CONTROL_PERIOD_MS));
  }
}

void interfaceTask(void* arg) {
  InterfaceState& state = *static_cast<InterfaceState*>(arg);

  for (;;) {
    const uint32_t now = millis();

    // The links answer the wire.
    state.link->poll(now);
    state.ble->poll(now);

    // A link that leaves is removed from the stack, and the control beat vents when the top
    // changes; the run does not outlive its owner.
    if (state.link->consumeLinkLost()) {
      state.context.ownership().drop(Link::Serial);
      state.logger->info("app: controller left");
    }

    // Connection presence, published for the panel's overlay.
    serialConnected = state.link->controllerAttached();
    bleConnected = state.ble->connected();

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void panelTask(void* arg) {
  PanelState& state = *static_cast<PanelState*>(arg);

  // A fresh boot starts with the panel on top: an external link that is present is only a reader
  // until it claims, so the panel's own page shows first.
  state.ownedShown = state.context.machine().ownership().external();
  state.ui = state.ownedShown ? static_cast<UserInterface*>(state.connected)
                              : static_cast<UserInterface*>(state.disconnected);
  if (state.ui) state.ui->begin();

  for (;;) {
    buttonsTick(state);

    // The screen follows ownership, not the cable: an external claim paints the link's page, and a
    // release hands the panel back. The hand-off vents the run, so the page that returns sees the
    // stop on its first poll.
    const bool external = state.context.machine().ownership().external();
    if (external != state.ownedShown) {
      state.ownedShown = external;
      stopSeq++;
      state.ui = external ? static_cast<UserInterface*>(state.connected)
                          : static_cast<UserInterface*>(state.disconnected);
      if (state.ui) state.ui->begin();
    }

    // The panel's own beat: the gauge, the buzzer and the store drain, then the glass.
    const uint32_t now = millis();
    state.context.battery().update(now);
    state.context.buzzer().update(now);
    state.context.storeServer().drain();

    // The overlay reports the ways in; the interface owner publishes the two booleans, and the
    // pages redraw themselves on the refresh cadence.
    state.context.overlay().setConnections(serialConnected, bleConnected);
    if (state.ui) state.ui->update();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void spawnMachineTasks(MachineContext& context, Logger& logger, SerialLink* link, BleLink* ble) {
  machineCommandQueue = xQueueCreate(MachineCommandQueueDepth, sizeof(MachineCommand));

  static ControlState control{context, &logger};
  static InterfaceState interface{context, &logger, link, ble};

  xTaskCreatePinnedToCore(controlTask, "control", ControlStackWords, &control, ControlPriority,
                          nullptr, ControlCore);
  xTaskCreatePinnedToCore(interfaceTask, "interface", InterfaceStackWords, &interface,
                          InterfacePriority, nullptr, InterfaceCore);
}

void spawnPanelTask(PanelContext& context, Logger& logger, ConnectedUI* connected,
                    DisconnectedUI* disconnected) {
  static PanelState panel{context, &logger, connected, disconnected};

  xTaskCreatePinnedToCore(panelTask, "panel", PanelStackWords, &panel, PanelPriority, nullptr,
                          PanelCore);
}

}  // namespace gardener
