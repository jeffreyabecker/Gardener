#include "app/App.h"

#include <Wire.h>
#include <stdlib.h>

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
  Storage::Mount mount = context_.storage().begin();
  context_.settings().begin();
  // The full device's composition reads the persisted preferences into the
  // machine's config; a machine-alone build hands constants instead.
  context_.config().bluetooth = context_.settings().bluetoothEnabled();
  context_.config().diagnostics = context_.settings().logEnabled();
  Logger* logger = context_.config().diagnostics
                       ? static_cast<Logger*>(new StreamLogger(Serial))
                       : static_cast<Logger*>(new NilLogger());
  SerialLink* link = new SerialLink(&context_, Serial, serialAttached, *logger);
  // The wire surface is the build's to register: the machine's five domains here, and a panel
  // build's own domains beside them.
  link->registerDomain(createControlDomain(context_));
  link->registerDomain(createDeviceDomain(context_));
  link->registerDomain(createVacuumDomain(context_));
  link->registerDomain(createBuzzerDomain(context_));
  link->registerDomain(createFsDomain(context_));
  BleLink* ble = new BleLink(&context_);
  ble->registerDomain(new MachineBleDomain(&context_));
  ble->registerDomain(new PanelBleDomain(&context_));
  ConnectedUI* connected = new ConnectedUI(context_);
  DisconnectedUI* disconnected = new DisconnectedUI(context_);

  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  logger->info(boot.c_str());

  // The panel is the one part with no fallback: nothing on the device can draw without it, so a
  // panel that does not answer is fatal rather than a degraded mode. Its bus is the app's to bring
  // up, because the driver takes a TwoWire it does not own. The sensor is an ADC pin, not a bus,
  // so it has nothing to bring up here.
  Wire1.begin(cfg::PANEL_I2C_SDA, cfg::PANEL_I2C_SCL, cfg::PANEL_I2C_FREQ_HZ);
  bool displayOk = context_.display().begin();
  logger->info(displayOk ? "oled: online" : "oled: not found, aborting");
  if (!displayOk) {
    // abort() panics and the ESP32 resets on a panic, so the device keeps trying rather than
    // coming up blind and silent.
    delay(50);
    abort();
  }

  bool sensorOk = context_.sensor().begin();
  bool pumpOk = context_.pump().begin();
  context_.vent().begin();
  bool batteryOk = context_.battery().begin();
  bool buzzerOk = context_.buzzer().begin();
  context_.vacuum().begin();

  // The mark is an image on the device rather than an array in a header. Every page draws it from
  // there when it needs it, so a data image without it is worth one line and nothing else — the pages
  // simply have no mark.
  if (context_.storage().size(kMarkAsset) == 0) {
    logger->warn("ui: /assets/eggplant.rle missing, the disconnected page has no mark");
  }

  // The interpreter comes up here and stays: routines are loaded into it later, and a device with
  // no routines simply never enters it.
  bool vmOk = context_.routines().begin(context_, context_.storage(), *logger,
                                        context_.storeClient());

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  logger->info(sensorOk ? "sensor: online" : "sensor: not found");
  logger->info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger->info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  logger->info(context_.vent().energiseWhenOpen() ? "vent: normally closed"
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
    usage += String((uint32_t)context_.storage().usedBytes());
    usage += " of ";
    usage += String((uint32_t)context_.storage().totalBytes());
    usage += " bytes used";
    logger->info(usage.c_str());
  }
  String stored = "settings: unit=";
  stored += displayUnitInfo(context_.settings().unit()).name;
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
  spawnTasks(context_, *logger, link, ble, &context_.storeServer(), connected, disconnected);
  return true;
}

}  // namespace gardener
