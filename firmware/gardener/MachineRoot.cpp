#include "MachineRoot.h"

#include "core/hal/PwmPump.h"
#include "core/hal/SolenoidValve.h"
#include "core/hal/XgzpSensor.h"

namespace gardener::runtime {

MachineParts buildMachineParts(const MachinePins& pins) {
  VentValve* valve = ValveNormallyOpen
                         ? static_cast<VentValve*>(new NormallyOpenValve(pins.ventValve))
                         : static_cast<VentValve*>(new NormallyClosedValve(pins.ventValve));
  MachineParts parts{
      new PwmPump(pins.pumpPwm, PumpDutyMaxPercent, PumpRampPercentPerSec),
      new AnalogSensor(pins.sensorAdc),
      valve,
  };
  return parts;
}

}  // namespace gardener::runtime