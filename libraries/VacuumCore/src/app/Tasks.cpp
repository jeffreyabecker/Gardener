#include "app/Tasks.h"

#include "app/AppContext.h"
#include "app/Logger.h"
#include "app/Ownership.h"
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
QueueHandle_t buttonEventQueue = nullptr;
volatile uint32_t stopSeq = 0;
volatile uint32_t goHomeSeq = 0;
volatile bool serialConnected = false;
volatile bool bleConnected = false;

namespace {

// Task configuration. The control task runs above the
// Arduino loop's default priority so the safety tick is never preempted by a link or the screen;
// the panel task runs lowest because a routine's poll may take its time. The panel's stack is the
// one the Berry interpreter runs inside.
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
constexpr UBaseType_t ButtonEventQueueDepth = 8;

// The keys in the order a press is reported, which is the panel's order rather than the bit order.
constexpr Button PressOrder[] = {Button::Up, Button::Down, Button::Ok, Button::Back};

// The local stop. It is a safety path rather than a preference, so it is a constant here and not a
// setting: holding Back for this long ends the run and vents, whatever the controller is doing.
constexpr uint32_t BackStopMs = 3000;

// The local reset. Back+OK together for this long release the chamber and restart, which is also
// how a controller that dropped while owning is cleared.
constexpr uint32_t ResetHoldMs = 5000;

// The control task's own state: the context it commands, the logger it reports through, and the
// latch/timing fields the run log and the holds need.
struct ControlState {
  AppContext context;
  Logger* logger;
  RunState lastState = RunState::Idle;
  uint32_t ventStartMs = 0;
  bool backStopFired = false;
  // The Up+Down chord: both keys were down at once, and the chord event has gone out. The pair's
  // own releases are not page presses while either flag is set.
  bool upDownChord = false;
  bool upDownChordFired = false;
};

// The panel task's own state: the two screens it owns, the one showing, and which page it is.
struct PanelState {
  AppContext context;
  Logger* logger;
  ConnectedUI* connected;
  DisconnectedUI* disconnected;
  UserInterface* ui = nullptr;
  bool ownedShown = false;
};

// The interface task's own state: the wire and the flash, and the logger for the link's reports.
struct InterfaceState {
  AppContext context;
  Logger* logger;
  SerialLink* link;
  BleLink* ble;
  StoreServer* storeServer;
};

// Applies one queued write to the machine. The op names the verb; the fields are the op's own.
void applyMachineCommand(ControlState& state, const MachineCommand& command) {
  AppContext& context = state.context;
  switch (command.op) {
    case MachineCommand::Op::Start: context.vacuum().start(); break;
    case MachineCommand::Op::Stop: context.vacuum().stop(); break;
    case MachineCommand::Op::ResetFault: context.vacuum().resetFault(); break;
    case MachineCommand::Op::SetTarget: context.vacuum().setTarget(command.value); break;
    case MachineCommand::Op::ClearTarget: context.vacuum().clearTarget(); break;
    case MachineCommand::Op::Reboot: safeReboot(context); break;
    case MachineCommand::Op::Tone: context.buzzer().tone((uint16_t)command.value, command.durationMs); break;
    case MachineCommand::Op::Play: context.buzzer().play(command.text.c_str()); break;
    case MachineCommand::Op::Off: context.buzzer().off(); break;
  }
}

// Every pending command, applied against the machine as the last tick left it.
void drainMachineCommands(ControlState& state) {
  MachineCommand command;
  while (xQueueReceive(machineCommandQueue, &command, 0) == pdTRUE) applyMachineCommand(state, command);
}

// The keys, settled and classified here: one ButtonEvent per completed press. The panel task
// consumes the queue. OK while the Back hold vents is not a page action: on the disconnected page it
// is the way home, so it is stolen here rather than queued. With a controller owning there is no
// page to go home to, so the press stays the routine-less log line it always was.
//
// Up and Down together are one chord, not two presses: the pair is swallowed and a single chord
// event is queued on the up key when the first of the two is released.
void buttonsTick(ControlState& state) {
  const bool upDown = state.context.buttons().pressed(Button::Up);
  const bool downDown = state.context.buttons().pressed(Button::Down);
  const bool bothDown = upDown && downDown;

  const bool backHeld = state.context.buttons().heldMs(Button::Back) >= BackStopMs;
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
    if (backHeld && !state.context.ownership().held() && button == Button::Ok) {
      goHomeSeq++;
      continue;
    }
    const ButtonEvent event{button, press};
    if (xQueueSend(buttonEventQueue, &event, 0) != pdTRUE) {
      state.logger->warn("buttons: queue full, dropping a press");
    }
  }

  // Advance the chord: both down arms it, the first release fires it, and both up clears it.
  if (bothDown && !state.upDownChordFired) {
    state.upDownChord = true;
  } else if (state.upDownChord && !state.upDownChordFired && !bothDown) {
    state.upDownChordFired = true;
    const ButtonEvent chordEvent{Button::Up, Press::Chord};
    if (xQueueSend(buttonEventQueue, &chordEvent, 0) != pdTRUE) {
      state.logger->warn("buttons: queue full, dropping a press");
    }
  }
  if (!upDown && !downDown) {
    state.upDownChord = false;
    state.upDownChordFired = false;
  }
}

// Back is a local stop, not a press with an action: held past the mark it stops the pump and vents,
// and the vent stays open as long as the key is down. The latch keeps the stopSeq bump — the
// panel's takeStopped() notice — to one per hold, while the stop itself is re-asserted every tick so
// a start or target command that sneaks in while Back is held is undone in the same tick. Releasing
// the key arms the next one and leaves the chamber vented.
void watchBackHold(ControlState& state) {
  const uint32_t held = state.context.buttons().heldMs(Button::Back);
  if (held == 0) {
    state.backStopFired = false;
    return;
  }
  if (held < BackStopMs) return;
  if (!state.backStopFired) {
    state.backStopFired = true;
    state.logger->warn("app: back held, venting");
    stopSeq++;
  }
  state.context.vacuum().stop();
}

// Both keys together are the local reset. It is what clears a record an absent controller left
// held, and it runs from every page whatever a controller is doing.
void watchResetHold(ControlState& state) {
  if (state.context.buttons().heldMs(Button::Back) < ResetHoldMs) return;
  if (state.context.buttons().heldMs(Button::Ok) < ResetHoldMs) return;
  state.logger->warn("app: back and ok held, resetting");
  safeReboot(state.context);
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

// The keys belong to whoever owns the page: an active routine takes the presses, and with none the
// panel logs them as it always has. A press is consumed either way, so it is never delivered twice.
void drainButtonEvents(PanelState& state) {
  const bool toRoutine = !state.context.ownership().held() && state.context.routines().active();
  ButtonEvent event;
  while (xQueueReceive(buttonEventQueue, &event, 0) == pdTRUE) {
    if (toRoutine) {
      state.context.routines().pushPress(event.button, event.press);
      continue;
    }
    String message = "btn: ";
    message += buttonName(event.button);
    message += ' ';
    message += pressName(event.press);
    state.logger->info(message.c_str());
  }
}

}  // namespace

void controlTask(void* arg) {
  ControlState& state = *static_cast<ControlState*>(arg);
  state.lastState = state.context.vacuum().runState();

  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    const uint32_t now = millis();

    // Commands first, against the machine as the last tick left it, then the devices, the PID and
    // the buzzer on the fixed beat, then the keys and the holds, then the run log.
    drainMachineCommands(state);
    state.context.sensor().update(now);
    state.context.pump().update(now);
    state.context.battery().update(now);
    state.context.vacuum().service(now);
    state.context.buzzer().update(now);
    buttonsTick(state);
    watchBackHold(state);
    watchResetHold(state);
    logRunChanges(state, now);

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(cfg::CONTROL_PERIOD_MS));
  }
}

void interfaceTask(void* arg) {
  InterfaceState& state = *static_cast<InterfaceState*>(arg);

  for (;;) {
    const uint32_t now = millis();

    // The store server applies the VM's flash and settings writes; the links answer the wire.
    state.storeServer->drain();
    state.link->poll(now);
    state.ble->poll(now);

    // A link that leaves while owning does not release the machine: its record stays held, the run
    // continues and the panel keeps showing that link. A reconnected controller stays a reader
    // until the local reset clears it.
    if (state.link->consumeLinkLost()) {
      if (state.context.ownership().ownedBy(Link::Serial)) {
        state.context.ownership().drop(Link::Serial);
        state.logger->warn("app: controller left, ownership held");
      } else {
        state.logger->info("app: controller left");
      }
    }

    // Connection presence, published for the panel's overlay.
    serialConnected = state.link->controllerAttached();
    bleConnected = state.ble->connected();

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void panelTask(void* arg) {
  PanelState& state = *static_cast<PanelState*>(arg);

  // A fresh boot has no controller: a link that is present is only a reader until it acquires
  // control, so the disconnected page shows first.
  state.ownedShown = state.context.ownership().held();
  state.ui = state.ownedShown ? static_cast<UserInterface*>(state.connected)
                              : static_cast<UserInterface*>(state.disconnected);
  if (state.ui) state.ui->begin();

  for (;;) {
    drainButtonEvents(state);

    // The screen follows ownership, not the cable: acquiring control paints the link's page, and a
    // drop while owning leaves it where it was. A swap paints its own first frame.
    const bool owned = state.context.ownership().held();
    if (owned != state.ownedShown) {
      state.ownedShown = owned;
      state.ui = owned ? static_cast<UserInterface*>(state.connected)
                       : static_cast<UserInterface*>(state.disconnected);
      if (state.ui) state.ui->begin();
    }

    // The overlay reports the ways in; the interface owner publishes the two booleans, and the
    // pages redraw themselves on the refresh cadence.
    state.context.overlay().setConnections(serialConnected, bleConnected);
    if (state.ui) state.ui->update();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void spawnTasks(AppContext& context, Logger& logger, SerialLink* link, BleLink* ble,
                StoreServer* storeServer, ConnectedUI* connected, DisconnectedUI* disconnected) {
  machineCommandQueue = xQueueCreate(MachineCommandQueueDepth, sizeof(MachineCommand));
  buttonEventQueue = xQueueCreate(ButtonEventQueueDepth, sizeof(ButtonEvent));

  static ControlState control{context, &logger};
  static InterfaceState interface{context, &logger, link, ble, storeServer};
  static PanelState panel{context, &logger, connected, disconnected};

  xTaskCreatePinnedToCore(controlTask, "control", ControlStackWords, &control, ControlPriority,
                          nullptr, ControlCore);
  xTaskCreatePinnedToCore(interfaceTask, "interface", InterfaceStackWords, &interface,
                          InterfacePriority, nullptr, InterfaceCore);
  xTaskCreatePinnedToCore(panelTask, "panel", PanelStackWords, &panel, PanelPriority, nullptr,
                          PanelCore);
}

}  // namespace gardener
