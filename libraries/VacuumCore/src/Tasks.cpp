#include "Tasks.h"

#include "Logger.h"
#include "Machine.h"
#include "Ownership.h"
#include "SafeReboot.h"
#include "VacuumSystem.h"
#include "ble/BleLink.h"
#include "serial/SerialLink.h"
#include "core/Config.h"

namespace gardener {

QueueHandle_t machineCommandQueue = nullptr;
volatile bool serialConnected = false;
volatile bool bleConnected = false;

namespace {

// Task configuration. The control task runs above the Arduino loop's default priority so the
// top-change vent and the PID are never preempted by a link or the screen.
constexpr UBaseType_t ControlPriority = 2;
constexpr UBaseType_t InterfacePriority = 1;
constexpr uint32_t ControlStackWords = 4096;
constexpr uint32_t InterfaceStackWords = 4096;
constexpr UBaseType_t ControlCore = 1;
constexpr UBaseType_t InterfaceCore = 0;

// Queue depths. Overflow drops the newest item with a log
// line; the consumers drain every tick, so a full queue is the exception, not the steady state.
constexpr UBaseType_t MachineCommandQueueDepth = 8;

// The control task's own state: the machine it commands, the logger it reports through, and the
// latch/timing fields the run log and the top-change vent need.
struct ControlState {
  Machine& machine;
  Logger* logger;
  RunState lastState = RunState::Idle;
  uint32_t ventStartMs = 0;
  Link lastOwner = Link::None;
};

// The interface task's own state: the machine it reports through, the links, and the logger.
struct InterfaceState {
  Machine& machine;
  Logger* logger;
  SerialLink* link;
  BleLink* ble;
};

// Applies one queued write to the machine. The op names the verb; the fields are the op's own.
void applyMachineCommand(ControlState& state, const MachineCommand& command) {
  Machine& machine = state.machine;
  switch (command.op) {
    case MachineCommand::Op::Start: machine.vacuum().start(); break;
    case MachineCommand::Op::Stop: machine.vacuum().stop(); break;
    case MachineCommand::Op::ResetFault: machine.vacuum().resetFault(); break;
    case MachineCommand::Op::SetTarget: machine.vacuum().setTarget(command.value); break;
    case MachineCommand::Op::ClearTarget: machine.vacuum().clearTarget(); break;
    case MachineCommand::Op::Reboot: safeReboot(machine); break;
  }
}

// Every pending command, applied against the machine as the last tick left it.
void drainMachineCommands(ControlState& state) {
  MachineCommand command;
  while (xQueueReceive(machineCommandQueue, &command, 0) == pdTRUE) applyMachineCommand(state, command);
}

// The drivers stopped narrating, so the run is reported from here: one line per state change,
// timed when it leaves a vent, plus the fault code when one latches.
void logRunChanges(ControlState& state, uint32_t nowMs) {
  const RunState current = state.machine.vacuum().runState();
  if (current == state.lastState) return;

  String message = runStateName(state.lastState);
  message += " -> ";
  message += runStateName(current);
  if (current == RunState::Fault) {
    message += " (fault ";
    message += state.machine.vacuum().faultCode();
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
  state.lastState = state.machine.vacuum().runState();
  state.lastOwner = state.machine.ownership().owner();

  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    const uint32_t now = millis();

    // Commands first, against the machine as the last tick left it.
    drainMachineCommands(state);

    // The top of the stack changed — a claim, a release or a drop all vent — so a new controller
    // starts from an open chamber.
    const Link top = state.machine.ownership().owner();
    if (top != state.lastOwner) {
      state.lastOwner = top;
      state.machine.vacuum().stop();
      String changed = "app: control to ";
      changed += linkName(top);
      state.logger->info(changed.c_str());
    }

    // The devices and the PID on the fixed beat, then the run log.
    state.machine.vacuum().sensor().update(now);
    state.machine.vacuum().pump().update(now);
    state.machine.vacuum().service(now);
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
      state.machine.ownership().drop(Link::Serial);
      state.logger->info("app: controller left");
    }

    // Connection presence, published for the panel's overlay.
    serialConnected = state.link->controllerAttached();
    bleConnected = state.ble->connected();

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void spawnMachineTasks(Machine& machine, SerialLink* link, BleLink* ble) {
  machineCommandQueue = xQueueCreate(MachineCommandQueueDepth, sizeof(MachineCommand));

  static ControlState control{machine, &machine.logger()};
  static InterfaceState interface{machine, &machine.logger(), link, ble};

  xTaskCreatePinnedToCore(controlTask, "control", ControlStackWords, &control, ControlPriority,
                          nullptr, ControlCore);
  xTaskCreatePinnedToCore(interfaceTask, "interface", InterfaceStackWords, &interface,
                          InterfacePriority, nullptr, InterfaceCore);
}

}  // namespace gardener
