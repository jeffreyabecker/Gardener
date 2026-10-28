#include "Build.h"

#if GARDENER_PANEL

#include "panel/Panel.h"

#include "core/Logger.h"
#include "panel/Units.h"
#include "storage/DeviceConfig.h"

namespace gardener {

// The panel's side of bring-up: the machine's parts first, then the panel's — the display, the gauge
// and buzzer, and the interpreter. The bus the display sits on, the links, the screens and the panel
// task belong to the composition that calls this, which is also what makes a missing display fatal.
bool Panel::boot() {
  Machine::boot();

  // The device's own documents come up before the interpreter, which reads the jingle library when
  // its modules are installed.
  config_.begin();
  jingles_.begin();

  Logger& log = logger();

  const bool displayOk = display().begin();
  log.info(displayOk ? "oled: online" : "oled: not found");

  const bool batteryOk = battery().begin();
  const bool buzzerOk = buzzer().begin();

  // The interpreter comes up here and stays: routines are loaded into it later, and a device with
  // no routines simply never enters it.
  const bool vmOk = routines().begin(*this);

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  log.info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  log.info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  log.info("buttons: four keys on interrupt");

  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(config_.get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  String stored = "settings: unit=";
  stored += displayUnitInfo(unit).name;
  log.info(stored.c_str());

  log.info(vmOk ? "vm: ready" : "vm: no memory, routines off");

  return displayOk;
}

}  // namespace gardener
#endif
