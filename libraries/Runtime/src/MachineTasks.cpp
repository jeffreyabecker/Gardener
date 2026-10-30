#include "MachineTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/Logger.h"
#include "core/SystemState.h"
#include "machine/ControlTick.h"
#include "machine/Machine.h"
#include "machine/SafeReboot.h"
#include "machine/VacuumSystem.h"
#include "ble/BleLink.h"
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

// The control task's own state: the tick it steps, and the machine the tick hands a Reboot back to.
// Everything a tick decides — the last state logged, the vent's start, the queue it drains — is the
// tick's, so this file keeps only the beat.
struct ControlState {
  Machine& machine;
  ControlTick& tick;
};

// The interface task's own state: the links it polls and the state it publishes their presence into.
struct InterfaceState {
  SerialLink* link;
  SystemState& published;
  BleLink* ble;
};

}  // namespace

void controlTask(void* arg) {
  ControlState& state = *static_cast<ControlState*>(arg);

  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    state.tick.tick(millis());
    if (state.tick.takeReboot()) safeReboot(state.machine);

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
  }
}

void interfaceTask(void* arg) {
  InterfaceState& state = *static_cast<InterfaceState*>(arg);

  for (;;) {
    const uint32_t now = millis();

    // The links answer the wire.
    state.link->poll(now);
    state.ble->poll(now);

    // Connection presence, published for the panel's overlay: a connected link that has not
    // acquired control still counts.
    state.published.serialConnected = state.link->controllerAttached();
    state.published.bluetoothConnected = state.ble->connected();

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link, BleLink* ble,
                       SystemState& state) {
  static ControlTick tick{machine.vacuum(), machine.logger(), queue, state};
  static ControlState control{machine, tick};
  static InterfaceState interface{link, state, ble};

  xTaskCreatePinnedToCore(controlTask, "control", ControlStackWords, &control, ControlPriority,
                          nullptr, ControlCore);
  xTaskCreatePinnedToCore(interfaceTask, "interface", InterfaceStackWords, &interface,
                          InterfacePriority, nullptr, InterfaceCore);
}

}  // namespace gardener
