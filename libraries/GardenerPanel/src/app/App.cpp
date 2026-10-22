#include "app/App.h"

#include <Wire.h>
#include <stdlib.h>

#include "app/Logger.h"
#include "app/NilLogger.h"
#include "app/PanelTasks.h"
#include "app/StreamLogger.h"
#include "app/ble/BleLink.h"
#include "app/ble/MachineBleDomain.h"
#include "app/ble/PanelBleDomain.h"
#include "app/panel/Asset.h"
#include "app/panel/ConnectedUI.h"
#include "app/panel/DisconnectedUI.h"
#include "app/serial/Domains.h"
#include "app/serial/PanelDomains.h"
#include "app/serial/SerialLink.h"
#include "core/Config.h"
#include "core/Units.h"

namespace gardener {
namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a
// plain UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

App::App(AppContext context) : context_(context) {}

// The panel's side of bring-up: the panel comes up — the display first, which is fatal when it
// does not answer — the gauge and buzzer follow, the interpreter starts, and the panel task takes
// over. The panel's wire domains are the composition's to register.
void panelBoot(PanelContext& context, Logger& logger, Storage::Mount mount) {
  ConnectedUI* connected = new ConnectedUI(context);
  DisconnectedUI* disconnected = new DisconnectedUI(context);

  // The panel is the one part with no fallback: nothing on the device can draw
  // without it, so a panel that does not answer is fatal rather than a degraded
  // mode.
  Wire1.begin(cfg::PANEL_I2C_SDA, cfg::PANEL_I2C_SCL, cfg::PANEL_I2C_FREQ_HZ);
  const bool displayOk = context.display().begin();
  logger.info(displayOk ? "oled: online" : "oled: not found, aborting");
  if (!displayOk) {
    // abort() panics and the ESP32 resets on a panic, so the device keeps
    // trying rather than coming up blind and silent.
    delay(50);
    abort();
  }

  const bool batteryOk = context.battery().begin();
  const bool buzzerOk = context.buzzer().begin();

  // The mark is an image on the device rather than an array in a header. Every page draws it from
  // there when it needs it, so a data image without it is worth one line and nothing else — the pages
  // simply have no mark.
  if (context.storage().size(kMarkAsset) == 0) {
    logger.warn("ui: /assets/eggplant.rle missing, the disconnected page has no mark");
  }

  // The interpreter comes up here and stays: routines are loaded into it later, and a device with
  // no routines simply never enters it.
  const bool vmOk = context.routines().begin(context, context.storage(), logger,
                                             context.storeClient());

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  logger.info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  logger.info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  logger.info("buttons: four keys on interrupt");

  switch (mount) {
    case Storage::Mount::Mounted:
      logger.info("fs: littlefs mounted");
      break;
    case Storage::Mount::Formatted:
      logger.info("fs: littlefs formatted, no filesystem found");
      break;
    case Storage::Mount::Failed:
      logger.warn("fs: littlefs mount failed, settings will not persist");
      break;
  }
  if (mount != Storage::Mount::Failed) {
    String usage = "fs: ";
    usage += String((uint32_t)context.storage().usedBytes());
    usage += " of ";
    usage += String((uint32_t)context.storage().totalBytes());
    usage += " bytes used";
    logger.info(usage.c_str());
  }
  String stored = "settings: unit=";
  stored += displayUnitInfo(context.settings().unit()).name;
  logger.info(stored.c_str());

  String vm = vmOk ? "vm: ready, heap " : "vm: no memory, routines off, heap ";
  vm += String((uint32_t)ESP.getFreeHeap());
  logger.info(vm.c_str());

  // The panel and the VM run on the panel task from here.
  spawnPanelTask(context, logger, connected, disconnected);
}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  // The files and the persisted knobs come up before either boot. Storage and settings are the
  // panel's, but the knobs they persist — the radio and the diagnostics — are the machine's, so the
  // composition reads them into the machine's config and picks the logger before the machine boots.
  Storage::Mount mount = context_.panel().storage().begin();
  context_.panel().settings().begin();
  context_.machine().config().bluetooth = context_.panel().settings().bluetoothEnabled();
  context_.machine().config().diagnostics = context_.panel().settings().logEnabled();
  Logger* logger = context_.machine().config().diagnostics
                       ? static_cast<Logger*>(new StreamLogger(Serial))
                       : static_cast<Logger*>(new NilLogger());

  // The machine first, then the panel — the composition builds and starts the links, the machine's
  // boot brings its parts up, and the panel registers on those same links. App is the full build, so
  // the panel always follows.
  SerialLink* serial = new SerialLink(Serial, serialAttached, *logger);
  serial->registerDomain(createControlDomain(context_.machine()));
  serial->registerDomain(createVacuumDomain(context_.machine()));
  serial->registerDomain(createDeviceDomain(context_.panel()));
  serial->registerDomain(createBuzzerDomain(context_.panel()));
  serial->registerDomain(createFsDomain(context_.panel()));
  BleLink* ble = new BleLink(&context_.machine());
  ble->registerDomain(new MachineBleDomain(&context_.machine()));
  ble->registerDomain(new PanelBleDomain(&context_.panel()));

  serial->begin();
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

  machineBoot(context_.machine(), *logger, serial, ble);
  panelBoot(context_.panel(), *logger, mount);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
  return true;
}

}  // namespace gardener
