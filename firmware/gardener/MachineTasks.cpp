#include "MachineTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "BleLink.h"
#include "beats/MachineTick.h"
#include "core/Knob.h"
#include "core/Logger.h"
#include "machine/Machine.h"
#include "machine/VacuumSystem.h"
#include "serial/SerialLink.h"

namespace gardener::runtime {

namespace {

// Task configuration. The control task runs above the Arduino loop's default priority so the PID is
// never preempted by a link or the screen. A part with one core runs every task and the loop on it,
// so the core knobs default to 0 and a two-core board names core 1 for the tick in its profile.
GARDENER_KNOB_VALID(UBaseType_t, ControlPriority, GARDENER_CONTROL_TASK_PRIORITY, 2,
                    GARDENER_CONTROL_TASK_PRIORITY <= 24,
                    "a task priority below the config ceiling");
GARDENER_KNOB_VALID(UBaseType_t, InterfacePriority, GARDENER_INTERFACE_TASK_PRIORITY, 1,
                    GARDENER_INTERFACE_TASK_PRIORITY <= 24,
                    "a task priority below the config ceiling");
GARDENER_KNOB_VALID(uint32_t, ControlStackWords, GARDENER_CONTROL_TASK_STACK_WORDS, 4096,
                    GARDENER_CONTROL_TASK_STACK_WORDS >= 1024 &&
                        GARDENER_CONTROL_TASK_STACK_WORDS <= 65536,
                    "the control task's stack, in words");
GARDENER_KNOB_VALID(uint32_t, InterfaceStackWords, GARDENER_INTERFACE_TASK_STACK_WORDS, 4096,
                    GARDENER_INTERFACE_TASK_STACK_WORDS >= 1024 &&
                        GARDENER_INTERFACE_TASK_STACK_WORDS <= 65536,
                    "the interface task's stack, in words");
GARDENER_KNOB_VALID(UBaseType_t, ControlCore, GARDENER_CONTROL_TASK_CORE, 0,
                    GARDENER_CONTROL_TASK_CORE < portNUM_PROCESSORS, "a core this part has");
GARDENER_KNOB_VALID(UBaseType_t, InterfaceCore, GARDENER_INTERFACE_TASK_CORE, 0,
                    GARDENER_INTERFACE_TASK_CORE < portNUM_PROCESSORS, "a core this part has");

// The control task's own state: the tick it steps. Everything a tick decides — the last state
// logged, the vent's start, the queue it drains — is the tick's, so this file keeps only the beat.
struct ControlState {
  MachineTick& tick;
};
// The interface task's own state: the links it polls.
struct InterfaceState {
  SerialLink* link;
  BleLink* bluetooth;
};

void controlTask(void* arg) {
  ControlState& state = *static_cast<ControlState*>(arg);

  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    state.tick.tick(millis());

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(ControlPeriodMs));
  }
}

void interfaceTask(void* arg) {
  InterfaceState& state = *static_cast<InterfaceState*>(arg);

  for (;;) {
    const uint32_t nowMs = millis();
    state.link->poll(nowMs);
    if (state.bluetooth) state.bluetooth->poll(nowMs);

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

void spawnMachineTasks(Machine& machine, CommandQueue& queue, SerialLink* link,
                       BleLink* bluetooth) {
  static MachineTick tick{machine.vacuum(), machine.logger(), queue};
  static ControlState control{tick};
  static InterfaceState interface{link, bluetooth};

  xTaskCreatePinnedToCore(controlTask, "control", ControlStackWords, &control, ControlPriority,
                          nullptr, ControlCore);
  xTaskCreatePinnedToCore(interfaceTask, "interface", InterfaceStackWords, &interface,
                          InterfacePriority, nullptr, InterfaceCore);
}

}  // namespace gardener::runtime