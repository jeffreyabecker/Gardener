#include "app/MachineBoot.h"

#include "app/Logger.h"
#include "core/Config.h"

namespace gardener {

// The machine's side of bring-up: the parts come up and report whether they answered. The links
// and the tasks belong to the composition that calls this.
void machineBoot(MachineContext& context, Logger& logger) {
  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  logger.info(boot.c_str());

  const bool sensorOk = context.sensor().begin();
  const bool pumpOk = context.pump().begin();
  context.vent().begin();
  context.vacuum().begin();

  logger.info(sensorOk ? "sensor: online" : "sensor: not found");
  logger.info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger.info(context.vent().energiseWhenOpen() ? "vent: normally closed"
                                                : "vent: normally open");
}

}  // namespace gardener
