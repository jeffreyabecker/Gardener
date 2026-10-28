#include "tasks/MachineTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/Logger.h"
#include "machine/Machine.h"
#include "machine/SafeReboot.h"
#include "machine/VacuumSystem.h"
#if GARDENER_BLUETOOTH
#include "ble/BleLink.h"
#endif
#include "serial/SerialLink.h"

namespace gardener {

namespace {

// Task configuration. The control task runs above the Arduino loop's default priority so the PID is
// never preempted by a link or the screen.
constexpr UBaseType_t ControlPriority = 2;
constexpr UBaseType_t InterfacePriority = 1;
constexpr uint32_t ControlStackWords = 4096;
constexpr uint32_t InterfaceStackWords = 4096;
constexpr UBaseType_t ControlCore = 1;
constexpr UBaseType_t InterfaceCore = 0;

// The control task's own state: the machine it commands, the logger it reports through, the queue
// it drains, and the latch/timing fields the run log needs. The queue is the composition's object —
// the links post into the same one — so this file is only where it is drained and the schedule is
// kept.
struct ControlState {
  Machine& machine;
  Logger* logger;
  CommandQueue& queue;
  RunState lastState = RunState::Idle;
  uint32_t ventStartMs = 0;
};

// The interface task's own state: the machine it reports through and the links it polls.
struct InterfaceState {
  Machine& machine;
  SerialLink* link;
#if GARDENER_BLUETOOTH
  BleLink* ble;
#endif
};

// Applies one queued write to the machine. The settings and the vacuum system are the machine's
// own; the reboot is the composition's, so it is the one left here.
void applyCommand(ControlState& state, const MachineCommand& command) {
  if (!applyMachineCommand(state.machine.vacuum(), command)) safeReboot(state.machine);
}

// A queued command the machine never had to apply, named with why, so a burst that collapses
// leaves the same evidence as one that ran.
void logCollapsed(ControlState& state, const MachineCommand& command, const char* reason) {
  String message = "queue: dropped ";
  message += machineCommandName(command.op);
  if (command.op == MachineCommand::Op::SetTarget) {
    message += ' ';
    message += String(command.value, 1u);
  }
  message += ' ';
  message += reason;
  state.logger->warn(message.c_str());
}

// Every pending command, collapsed to what the machine should do rather than replayed step by
// step: a stop discards everything queued before it, and repeated target writes reduce to the last
// one. A burst therefore cannot walk the valve back and forth, and a start queued before a stop
// never runs after it.
void drainMachineCommands(ControlState& state) {
  MachineCommand batch[CommandQueueDepth];
  uint8_t count = 0;
  while (count < CommandQueueDepth && state.queue.take(batch[count])) {
    count++;
  }
  if (count == 0) return;

  // The last stop owns everything before it.
  uint8_t first = 0;
  for (uint8_t i = count; i > 0; i--) {
    if (batch[i - 1].op == MachineCommand::Op::Stop) {
      first = i - 1;
      break;
    }
  }
  for (uint8_t i = 0; i < first; i++) logCollapsed(state, batch[i], "before a stop");

  // Of what is left, only the last target is worth applying: the depths before it are setpoints
  // the machine never has to stand still at.
  uint8_t lastTarget = count;
  for (uint8_t i = count; i > first; i--) {
    if (batch[i - 1].op == MachineCommand::Op::SetTarget) {
      lastTarget = i - 1;
      break;
    }
  }
  for (uint8_t i = first; i < count; i++) {
    if (i != lastTarget && batch[i].op == MachineCommand::Op::SetTarget) {
      logCollapsed(state, batch[i], "for a later target");
      continue;
    }
    applyCommand(state, batch[i]);
  }
}

// The drivers stopped narrating, so the run is reported from here: one line per state change,
// timed when it leaves a vent, plus the fault code when one latches and the deadline when one
// ends the run.
void logRunChanges(ControlState& state, uint32_t nowMs) {
  if (state.machine.vacuum().takeDeadlineExpired()) {
    String deadline = "run: hold deadline reached after ";
    deadline += String(state.machine.vacuum().holdSeconds());
    deadline += " s";
    state.logger->warn(deadline.c_str());
  }

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
    message += " after ";
    message += String(nowMs - state.ventStartMs);
    message += " ms";
  }
  if (current == RunState::Venting) state.ventStartMs = nowMs;
  state.logger->info(message.c_str());
  state.lastState = current;
}

}  // namespace

void controlTask(void* arg) {
  ControlState& state = *static_cast<ControlState*>(arg);
  state.lastState = state.machine.vacuum().runState();

  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    const uint32_t now = millis();

    // Commands first, against the machine as the last tick left it.
    drainMachineCommands(state);

    // The devices and the PID on the fixed beat, then the run log.
    state.machine.vacuum().sensor().update(now);
    state.machine.vacuum().pump().update(now);
    state.machine.vacuum().service(now);
    logRunChanges(state, now);

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
  }
}

void interfaceTask(void* arg) {
  InterfaceState& state = *static_cast<InterfaceState*>(arg);

  for (;;) {
    const uint32_t now = millis();

    // The links answer the wire.
    state.link->poll(now);
#if GARDENER_BLUETOOTH
    state.ble->poll(now);
#endif

    // Connection presence, published for the panel's band: a connected link that has not acquired
    // control still counts, and a build without the radio reports none.
#if GARDENER_BLUETOOTH
    state.machine.setPresence(state.link->controllerAttached(), state.ble->connected());
#else
    state.machine.setPresence(state.link->controllerAttached(), false);
#endif

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

#if GARDENER_BLUETOOTH
void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link, BleLink* ble) {
#else
void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link) {
#endif
  static ControlState control{machine, &machine.logger(), queue};
#if GARDENER_BLUETOOTH
  static InterfaceState interface{machine, link, ble};
#else
  static InterfaceState interface{machine, link};
#endif

  xTaskCreatePinnedToCore(controlTask, "control", ControlStackWords, &control, ControlPriority,
                          nullptr, ControlCore);
  xTaskCreatePinnedToCore(interfaceTask, "interface", InterfaceStackWords, &interface,
                          InterfacePriority, nullptr, InterfaceCore);
}

}  // namespace gardener
