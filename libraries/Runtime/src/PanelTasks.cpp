#include "PanelTasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "machine/SafeReboot.h"
#include "core/SystemState.h"
#include "machine/MachineCommand.h"
#include "routines/ButtonsTick.h"
#include "routines/PartsTick.h"
#include "routines/RoutinesTick.h"

namespace gardener {

namespace {

// The panel task runs lowest because a routine's poll may take its time, and its stack is the one
// the Berry interpreter runs inside.
constexpr UBaseType_t PanelPriority = 0;
constexpr uint32_t PanelStackWords = 8192;
constexpr UBaseType_t PanelCore = 0;
constexpr uint32_t PanelBeatMs = 10;

// The panel task's own state: the beats it steps, the outbox it flushes and the machine it reboots.
struct PanelState {
  CommandOutbox& outbox;
  Machine& machine;
  ButtonsTick& buttons;
  PartsTick& parts;
  RoutinesTick& page;
};

}  // namespace

void panelTask(void* arg) {
  PanelState& state = *static_cast<PanelState*>(arg);

  state.page.begin();

  for (;;) {
    // A command the panel could not hand to the control task on its last beat is retried first.
    state.outbox.flush();
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

void spawnPanelTask(ButtonPanel& buttonsPart, panel::GestureQueue& presses, CommandOutbox& outbox,
                    Logger& logger, Battery& battery, panel::parts::BuzzerPlayer& buzzer,
                    Routines& routines, Machine& machine, SystemState& state) {
  static ButtonsTick buttons{buttonsPart, presses, outbox, logger};
  static PartsTick parts{battery, buzzer, state};
  static RoutinesTick page{routines};
  static PanelState panel{outbox, machine, buttons, parts, page};

  xTaskCreatePinnedToCore(panelTask, "panel", PanelStackWords, &panel, PanelPriority, nullptr,
                          PanelCore);
}

}  // namespace gardener
