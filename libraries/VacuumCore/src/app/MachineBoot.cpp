#include "app/MachineBoot.h"

#include "app/Logger.h"
#include "app/Tasks.h"
#include "app/ble/BleLink.h"
#include "app/serial/SerialLink.h"
#include "core/Config.h"

namespace gardener {

// The machine's side of bring-up. The links arrive built, populated and started by the caller; all
// this does is bring the machine's parts up, report what came up, and start the tasks that drive
// them.
void machineBoot(MachineContext& context, Logger& logger, SerialLink* serial, BleLink* ble) {
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

  // The machine owns everything from here: the control task drives the
  // pressure loop, the interface task answers the links.
  spawnMachineTasks(context, logger, serial, ble);
}

}  // namespace gardener
