#include "app/Build.h"

#include <Wire.h>

#include "app/AppContext.h"
#include "app/Overlay.h"
#include "app/VacuumSystem.h"
#include "hal/impl/AdafruitDisplay.h"
#include "hal/impl/AdcBattery.h"
#include "hal/impl/Buttons.h"
#include "hal/impl/MprlsSensor.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"

namespace gardener {

// The graph is new'd once, at boot, and never deleted: the device's destructor never runs, so
// nothing owns it — the same policy the SH1106 driver already documents. Building it here rather
// than as value members means the wiring reads top to bottom in one place, and no class's
// declaration order has to match its dependencies.
AppContext* buildAppContext() {
  auto* sensor = new MprlsSensor(Wire);
  auto* pump = new PwmPump;
  auto* vent = new NormallyOpenValve;
  auto* battery = new AdcBattery;
  auto* buttons = new Buttons;
  auto* display = new AdafruitDisplay;
  auto* overlay = new Overlay(display, battery);
  auto* system = new VacuumSystem(pump, sensor, vent);
  return new AppContext(sensor, pump, vent, battery, buttons, overlay, system);
}

}  // namespace gardener
