#include "panel/Panel.h"

#include "core/Logger.h"
#include "core/Units.h"

namespace gardener::panel {

// The panel's side of bring-up: the machine's parts first, then the panel's — the display, the gauge
// and buzzer, and the interpreter. The bus the display sits on, the links, the screens and the panel
// task belong to the composition that calls this, which is also what makes a missing display fatal.
bool Panel::boot() {
  machine_.boot();

  // The device's own document comes up before the interpreter, which reads it when its modules are
  // installed.
  settings_.begin();

  Logger& log = logger();

  const bool displayOk = display().begin();
  log.info(displayOk ? "oled: online" : "oled: not found");

  const bool batteryOk = battery().begin();
  const bool buzzerOk = buzzer().begin();

  // The VM came up with the interpreter; the image is loaded into it here, and a device with no
  // routines simply never enters it.
  const bool vmOk = routines().begin();

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  log.info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  log.info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  log.info("buttons: four keys on interrupt");

  String stored = "settings: unit=";
  stored += displayUnitInfo(settings_.unit()).name;
  log.info(stored.c_str());

  log.info(vmOk ? "vm: ready" : "vm: no memory, routines off");

  return displayOk;
}

}  // namespace gardener::panel
