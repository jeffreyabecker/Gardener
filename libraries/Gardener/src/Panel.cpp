#include "Build.h"

#if GARDENER_PANEL

#include "Panel.h"

#include <Wire.h>
#include <stdlib.h>

#include "Logger.h"
#include "panel/Units.h"
#include "storage/DeviceConfig.h"

namespace gardener {

// The panel's side of bring-up: the machine's parts first, then the panel's — the display, which is
// fatal when it does not answer, the gauge and buzzer, and the interpreter. The links, the screens
// and the panel task belong to the composition that calls this.
void Panel::boot() {
  Machine::boot();

  // The device's own documents come up before the interpreter, which reads the jingle library when
  // its modules are installed.
  config_.begin();
  jingles_.begin();

  Logger& log = logger();

  // The panel is the one part with no fallback: nothing on the device can draw
  // without it, so a panel that does not answer is fatal rather than a degraded
  // mode.
  Wire1.begin(pins_.i2cSda, pins_.i2cScl, pins_.i2cHz);
  const bool displayOk = display().begin();
  log.info(displayOk ? "oled: online" : "oled: not found, aborting");
  if (!displayOk) {
    // abort() panics and the ESP32 resets on a panic, so the device keeps
    // trying rather than coming up blind and silent.
    delay(50);
    abort();
  }

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

  String vm = vmOk ? "vm: ready, heap " : "vm: no memory, routines off, heap ";
  vm += String((uint32_t)ESP.getFreeHeap());
  log.info(vm.c_str());
}

}  // namespace gardener
#endif
