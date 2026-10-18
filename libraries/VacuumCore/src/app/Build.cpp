#include "app/Build.h"

#include <Wire.h>

#include "app/AppContext.h"
#include "app/Overlay.h"
#include "app/VacuumSystem.h"
#include "core/Config.h"
#include "hal/impl/AdafruitDisplay.h"
#include "hal/impl/AdcBattery.h"
#include "hal/impl/Buttons.h"
#include "hal/impl/MprlsSensor.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"

namespace gardener {

// The parts are new'd once, at boot, and never deleted: the device's destructor never runs, so
// nothing owns them — the same policy the SH1106 driver already documents. This is the one place
// that names the rig: which pin drives what, which bus the two I2C parts share, and which valve is
// fitted. The drivers are handed those facts rather than reading them, so a board variant edits
// this file and core/Config.h and nothing under hal/impl/. The AppContext itself is copied out by
// value.
AppContext buildAppContext() {
  auto* sensor = new MprlsSensor(Wire);
  auto* pump = new PwmPump(cfg::PUMP_PWM);
  auto* vent = new NormallyOpenValve(cfg::VENT_VALVE);
  auto* battery = new AdcBattery(cfg::BATTERY_ADC);
  auto* buttons = new Buttons(cfg::BTN_UP, cfg::BTN_DOWN, cfg::BTN_OK, cfg::BTN_BACK);
  auto* display = new AdafruitDisplay(Wire, cfg::I2C_SDA, cfg::I2C_SCL, cfg::I2C_FREQ_HZ);
  auto* overlay = new Overlay(display, battery);
  auto* system = new VacuumSystem(pump, sensor, vent);
  return AppContext(sensor, pump, vent, battery, buttons, overlay, system);
}

}  // namespace gardener
