#include "app/MachineBoot.h"

#include "app/Logger.h"
#include "app/Tasks.h"
#include "app/ble/BleLink.h"
#include "app/ble/MachineBleDomain.h"
#include "app/serial/SerialLink.h"
#include "core/Config.h"

namespace gardener {

// The machine's side of bring-up. It brings the machine's parts up, builds and starts the BLE link
// with the machine's own domain on it, and hands the link back so the panel's boot can register its
// domains beside the machine's. The serial link arrives built and populated by the caller.
MachineBoot machineBoot(MachineContext& context, Logger& logger, SerialLink* link) {
  BleLink* ble = new BleLink(&context);
  ble->registerDomain(new MachineBleDomain(&context));

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

  link->begin();
  // The radio is a bonus transport: a device with the preference off stays silent, and a failure is
  // a line here rather than a boot that does not come up. It comes up before the ready line so that
  // line reports the heap the firmware actually runs with.
  switch (ble->begin()) {
    case BleLink::Start::Advertising:
      logger.info("ble: advertising as gardener");
      break;
    case BleLink::Start::Disabled:
      logger.info("ble: off");
      break;
    case BleLink::Start::Failed:
      logger.warn("ble: start failed, serial only");
      break;
  }

  // The machine owns everything from here: the control task drives the
  // pressure loop, the interface task answers the links.
  spawnMachineTasks(context, logger, link, ble);
  return {link, ble};
}

}  // namespace gardener
