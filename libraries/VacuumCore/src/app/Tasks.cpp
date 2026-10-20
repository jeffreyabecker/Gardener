#include "app/Tasks.h"

namespace gardener {

QueueHandle_t machineCommandQueue = nullptr;
QueueHandle_t storeQueue = nullptr;
QueueHandle_t buttonEventQueue = nullptr;
volatile uint32_t okHeldMs = 0;
volatile uint32_t stopSeq = 0;

namespace {

// Task configuration, from docs/plans/free-rtos/tasks.md. The control task runs above the
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

// Queue depths, from docs/plans/free-rtos/queues.md. Overflow drops the newest item with a log
// line; the consumers drain every tick, so a full queue is the exception, not the steady state.
constexpr UBaseType_t MachineCommandQueueDepth = 8;
constexpr UBaseType_t ButtonEventQueueDepth = 8;

}  // namespace

// The three task bodies are stubs through the foundation epic: the queues and the pinning exist,
// and the loop still runs the machine until the later epics move each half of App::update() in.
void controlTask(void* arg) {
  (void)arg;
  for (;;) vTaskDelay(pdMS_TO_TICKS(50));
}

void interfaceTask(void* arg) {
  (void)arg;
  for (;;) vTaskDelay(pdMS_TO_TICKS(5));
}

void panelTask(void* arg) {
  (void)arg;
  for (;;) vTaskDelay(pdMS_TO_TICKS(10));
}

void spawnTasks(AppContext& context) {
  machineCommandQueue = xQueueCreate(MachineCommandQueueDepth, sizeof(MachineCommand));
  buttonEventQueue = xQueueCreate(ButtonEventQueueDepth, sizeof(ButtonEvent));

  static TaskState control{context};
  static TaskState interface{context};
  static TaskState panel{context};

  xTaskCreatePinnedToCore(controlTask, "control", ControlStackWords, &control, ControlPriority,
                          nullptr, ControlCore);
  xTaskCreatePinnedToCore(interfaceTask, "interface", InterfaceStackWords, &interface,
                          InterfacePriority, nullptr, InterfaceCore);
  xTaskCreatePinnedToCore(panelTask, "panel", PanelStackWords, &panel, PanelPriority, nullptr,
                          PanelCore);
}

}  // namespace gardener
