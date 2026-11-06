#include "PanelTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "beats/ButtonsTick.h"
#include "beats/PartsTick.h"
#include "beats/RoutinesTick.h"
#include "core/Knob.h"
#include "core/SystemState.h"
#include "machine/SafeReboot.h"

namespace gardener::runtime {

namespace {

// The panel task runs lowest because a routine's poll may take its time, and its stack is the one
// the Berry interpreter runs inside.
GARDENER_KNOB_VALID(UBaseType_t, PanelPriority, GARDENER_PANEL_TASK_PRIORITY, 0,
                    GARDENER_PANEL_TASK_PRIORITY <= 24, "a task priority below the config ceiling");
GARDENER_KNOB_VALID(uint32_t, PanelStackWords, GARDENER_PANEL_TASK_STACK_WORDS, 8192,
                    GARDENER_PANEL_TASK_STACK_WORDS >= 1024 &&
                        GARDENER_PANEL_TASK_STACK_WORDS <= 65536,
                    "the panel task's stack, in words");
GARDENER_KNOB_VALID(UBaseType_t, PanelCore, GARDENER_PANEL_TASK_CORE, 0,
                    GARDENER_PANEL_TASK_CORE <= 1, "the part has two cores");
GARDENER_KNOB_VALID(uint32_t, PanelBeatMs, GARDENER_PANEL_BEAT_MS, 10,
                    GARDENER_PANEL_BEAT_MS >= 1 && GARDENER_PANEL_BEAT_MS <= 1000,
                    "the panel's beat, in milliseconds");

// The panel task's own state: the beats it steps and the machine it reboots.
struct PanelState {
  Machine& machine;
  ButtonsTick& buttons;
  PartsTick& parts;
  RoutinesTick& page;
};

}  // namespace

void panelTask(void* arg) {
  PanelState& state = *static_cast<PanelState*>(arg);

  state.buttons.begin();
  state.parts.begin();
  state.page.begin();

  for (;;) {
    state.buttons.tick();

    // The reset stops and vents before anything else on this beat draws.
    if (state.buttons.takeReset()) safeReboot(state.machine);

    // The panel's own beat: the gauge and the buzzer, then the glass.
    const uint32_t now = millis();
    state.parts.tick(now);

    // The device's force-quit stops and vents from the panel task and asks for its root page; the
    // page goes back to `default` then. The stack's own beat is skipped for that pass, so the page
    // that just left is not drawn again on the way out.
    if (state.buttons.takeHome()) {
      state.page.home();
    } else {
      state.page.tick(now);
    }

    vTaskDelay(pdMS_TO_TICKS(PanelBeatMs));
  }
}

void spawnPanelTask(ButtonPanel& buttonsPart, GestureQueue& presses, CommandQueue& queue,
                    Logger& logger, Battery& battery, BuzzerPlayer& buzzer, Routines& routines,
                    Machine& machine, SystemState& state) {
  static ButtonsTick buttons{buttonsPart, presses, queue, logger};
  static PartsTick parts{battery, buzzer, state};
  static RoutinesTick page{routines};
  static PanelState panel{machine, buttons, parts, page};

  xTaskCreatePinnedToCore(panelTask, "panel", PanelStackWords, &panel, PanelPriority, nullptr,
                          PanelCore);
}

}  // namespace gardener::runtime