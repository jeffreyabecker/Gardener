#include "app/App.h"

#include <Wire.h>
#include <stdlib.h>

#include "app/Build.h"
#include "app/Logger.h"
#include "app/NilLogger.h"
#include "app/Ownership.h"
#include "app/StoreBus.h"
#include "app/StreamLogger.h"
#include "app/Tasks.h"
#include "app/ble/BleLink.h"
#include "app/ble/MachineBleDomain.h"
#include "app/ble/PanelBleDomain.h"
#include "app/panel/Asset.h"
#include "app/panel/ConnectedUI.h"
#include "app/panel/DisconnectedUI.h"
#include "app/serial/SerialLink.h"
#include "app/serial/Domains.h"
#include "core/Config.h"
#include "core/Units.h"

namespace gardener {
namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a
// plain UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

App::App(AppContext context) : context_(context) {}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  // The device's files are mounted once, before anything that reads a preference — including the
  // logger, which the persisted `log` switch picks. A partition with no filesystem is formatted,
  // which is reported rather than assumed: whatever was there is gone.
  Storage::Mount mount = context_.panel().storage().begin();
  context_.panel().settings().begin();
  // The full device's composition reads the persisted preferences into the
  // machine's config; a machine-alone build hands constants instead.
  context_.machine().config().bluetooth = context_.panel().settings().bluetoothEnabled();
  context_.machine().config().diagnostics = context_.panel().settings().logEnabled();
  Logger* logger = context_.machine().config().diagnostics
                       ? static_cast<Logger*>(new StreamLogger(Serial))
                       : static_cast<Logger*>(new NilLogger());
  SerialLink* link = new SerialLink(Serial, serialAttached, *logger);
  // The wire surface is the build's to register: the machine's domains on the machine context, and
  // the panel's own domains on the panel context beside them.
  link->registerDomain(createControlDomain(context_.machine()));
  link->registerDomain(createDeviceDomain(context_.panel()));
  link->registerDomain(createVacuumDomain(context_.machine()));
  link->registerDomain(createBuzzerDomain(context_.panel()));
  link->registerDomain(createFsDomain(context_.panel()));
  BleLink* ble = new BleLink(&context_.machine());
  ble->registerDomain(new MachineBleDomain(&context_.machine()));
  ble->registerDomain(new PanelBleDomain(&context_.panel()));
  ConnectedUI* connected = new ConnectedUI(context_.panel());
  DisconnectedUI* disconnected = new DisconnectedUI(context_.panel());

  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  logger->info(boot.c_str());

  // The panel, when one is fitted, is the full build's to bring up — the
  // machine's boot names no panel part. A machine-alone build reports no panel
  // in its capability and skips this, coming up with no display.
  if (context_.machine().capabilities().panel()) {
    panelBoot(context_.panel(), *logger);
  }

  bool sensorOk = context_.machine().sensor().begin();
  bool pumpOk = context_.machine().pump().begin();
  context_.machine().vent().begin();
  bool batteryOk = context_.panel().battery().begin();
  bool buzzerOk = context_.panel().buzzer().begin();
  context_.machine().vacuum().begin();

  // The mark is an image on the device rather than an array in a header. Every page draws it from
  // there when it needs it, so a data image without it is worth one line and nothing else — the pages
  // simply have no mark.
  if (context_.panel().storage().size(kMarkAsset) == 0) {
    logger->warn("ui: /assets/eggplant.rle missing, the disconnected page has no mark");
  }

  // The interpreter comes up here and stays: routines are loaded into it later, and a device with
  // no routines simply never enters it.
  bool vmOk = context_.panel().routines().begin(context_.panel(), context_.panel().storage(),
                                                *logger, context_.panel().storeClient());

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  logger->info(sensorOk ? "sensor: online" : "sensor: not found");
  logger->info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger->info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  logger->info(context_.machine().vent().energiseWhenOpen() ? "vent: normally closed"
                                                            : "vent: normally open");
  logger->info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  logger->info("buttons: four keys on interrupt");

  switch (mount) {
    case Storage::Mount::Mounted:
      logger->info("fs: littlefs mounted");
      break;
    case Storage::Mount::Formatted:
      logger->info("fs: littlefs formatted, no filesystem found");
      break;
    case Storage::Mount::Failed:
      logger->warn("fs: littlefs mount failed, settings will not persist");
      break;
  }
  if (mount != Storage::Mount::Failed) {
    String usage = "fs: ";
    usage += String((uint32_t)context_.panel().storage().usedBytes());
    usage += " of ";
    usage += String((uint32_t)context_.panel().storage().totalBytes());
    usage += " bytes used";
    logger->info(usage.c_str());
  }
  String stored = "settings: unit=";
  stored += displayUnitInfo(context_.panel().settings().unit()).name;
  logger->info(stored.c_str());

  String vm = vmOk ? "vm: ready, heap " : "vm: no memory, routines off, heap ";
  vm += String((uint32_t)ESP.getFreeHeap());
  logger->info(vm.c_str());

  link->begin();
  // The radio is a bonus transport: a device with the preference off stays silent, and a failure is
  // a line here rather than a boot that does not come up. It comes up before the ready line so that
  // line reports the heap the firmware actually runs with.
  switch (ble->begin()) {
    case BleLink::Start::Advertising:
      logger->info("ble: advertising as gardener");
      break;
    case BleLink::Start::Disabled:
      logger->info("ble: off");
      break;
    case BleLink::Start::Failed:
      logger->warn("ble: start failed, serial only");
      break;
  }

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());

  // The tasks own everything from here: the machine on the control task, the wire on the
  // interface task, the panel and the VM on the panel task.
  spawnTasks(context_, *logger, link, ble, connected, disconnected);
  return true;
}

}  // namespace gardener
