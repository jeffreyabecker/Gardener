#include "app/Build.h"

#include "app/Capabilities.h"
#include "app/Ownership.h"
#include "app/VacuumSystem.h"
#include "core/Config.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"
#include "hal/impl/XgzpSensor.h"

namespace gardener {

// The machine's own composition: the pressure loop's parts, the ownership stack and the
// capability bits every build shares. The parts are new'd once, at boot, and never deleted: the
// device's destructor never runs, so nothing owns them. The sensor, pump and vent are named here
// with their pins, so a board variant edits this file and core/Config.h and nothing under
// hal/impl/.
MachineContext& buildMachineRoot() {
  auto* sensor = new XgzpSensor(cfg::SENSOR_ADC);
  auto* pump = new PwmPump(cfg::PUMP_PWM);
  auto* vent = new NormallyOpenValve(cfg::VENT_VALVE);
  auto* system = new VacuumSystem(pump, sensor, vent);
  auto* ownership = new Ownership();
  auto* capabilities = new Capabilities();
  // Like every build, the machine vents when its owner leaves.
  capabilities->setOwnershipStop(true);
  return *new MachineContext(sensor, pump, vent, system, ownership, capabilities);
}

}  // namespace gardener
