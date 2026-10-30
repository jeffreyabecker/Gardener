#include "MachineRoot.h"

#include "MachineRoot.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"
#include "hal/impl/XgzpSensor.h"

namespace gardener {
namespace {

VentValve* makeValve(uint8_t pin) {
  if (VALVE_NORMALLY_OPEN) return new NormallyOpenValve(pin);
  return new NormallyClosedValve(pin);
}

}  // namespace

MachineParts buildMachineParts(const MachinePins& pins) {
  MachineParts parts{
      new PwmPump(pins.pumpPwm, PUMP_DUTY_MAX_PERCENT, PUMP_RAMP_PERCENT_PER_SEC),
      new XgzpSensor(pins.sensorAdc),
      makeValve(pins.ventValve),
  };
  return parts;
}

}  // namespace gardener
