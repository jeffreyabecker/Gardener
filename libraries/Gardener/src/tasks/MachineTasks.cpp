#include "tasks/MachineTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/Logger.h"
#include "machine/ControlTick.h"
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

// The control task's own state: the tick it steps, and the machine the tick hands a Reboot back to.
// Everything a tick decides — the last state logged, the vent's start, the queue it drains — is the
// tick's, so this file keeps only the beat.
struct ControlState {
  Machine& machine;
  ControlTick& tick;
};

// The interface task's own state: the machine it reports through and the links it polls.
struct InterfaceState {
  Machine& machine;
  SerialLink* link;
#if GARDENER_BLUETOOTH
  BleLink* ble;
#endif
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
  static ControlTick tick{machine.vacuum(), machine.logger(), queue};
  static ControlState control{machine, tick};
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
