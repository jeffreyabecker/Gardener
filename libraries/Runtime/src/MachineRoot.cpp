#include "MachineRoot.h"

#include "MachineRoot.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"
#include "hal/impl/XgzpSensor.h"

namespace gardener::runtime {
namespace {

VentValve* makeValve(uint8_t pin) {
  if (ValveNormallyOpen) return new NormallyOpenValve(pin);
  return new NormallyClosedValve(pin);
}

}  // namespace

MachineParts buildMachineParts(const MachinePins& pins) {
  MachineParts parts{
      new PwmPump(pins.pumpPwm, PumpDutyMaxPercent, PumpRampPercentPerSec),
      new XgzpSensor(pins.sensorAdc),
      makeValve(pins.ventValve),
  };
  return parts;
}

}  // namespace gardener::runtime