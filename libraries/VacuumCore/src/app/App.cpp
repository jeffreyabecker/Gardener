#include "app/App.h"

#include <Wire.h>
#include <stdlib.h>

#include "app/Logger.h"
#include "app/NilLogger.h"
#include "app/Ownership.h"
#include "app/StoreBus.h"
#include "app/StreamLogger.h"
#include "app/Tasks.h"
#include "app/panel/Asset.h"
#include "core/Config.h"
#include "core/Units.h"

namespace gardener {
namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a
// plain UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

App::App(AppContext context) : context_(context) {
  ble_ = new BleLink(&context_);
  connectedUi_ = new ConnectedUI(context_);
}

bool App::begin() {
  Serial.begin(cfg::SERIAL_BAUD);
  delay(200);

  // The device's files are mounted once, before anything that reads a preference — including the
  // logger, which the persisted `log` switch picks. A partition with no filesystem is formatted,
  // which is reported rather than assumed: whatever was there is gone.
  Storage::Mount mount = context_.storage().begin();
  context_.settings().begin();
  logger_ = context_.settings().logEnabled() ? static_cast<Logger*>(new StreamLogger(Serial))
                                             : static_cast<Logger*>(new NilLogger());
  link_ = new SerialLink(&context_, Serial, serialAttached, *logger_);
  disconnectedUi_ = new DisconnectedUI(context_, *logger_);

  String boot = "gardener ";
  boot += cfg::FIRMWARE_VERSION;
  boot += " booting";
  logger_->info(boot.c_str());

  // The panel is the one part with no fallback: nothing on the device can draw without it, so a
  // panel that does not answer is fatal rather than a degraded mode. Its bus is the app's to bring
  // up, because the driver takes a TwoWire it does not own. The sensor is an ADC pin, not a bus,
  // so it has nothing to bring up here.
  Wire1.begin(cfg::PANEL_I2C_SDA, cfg::PANEL_I2C_SCL, cfg::PANEL_I2C_FREQ_HZ);
  bool displayOk = context_.display().begin();
  logger_->info(displayOk ? "oled: online" : "oled: not found, aborting");
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
    logger_->warn("ui: /assets/eggplant.rle missing, the disconnected page has no mark");
  }

  // The interpreter comes up here and stays: routines are loaded into it later, and a device with
  // no routines simply never enters it.
  bool vmOk = context_.routines().begin(context_, context_.storage(), *logger_);

  // What came up, and what did not. The devices report as values and one place decides what is
  // worth saying.
  logger_->info(sensorOk ? "sensor: online" : "sensor: not found");
  logger_->info(pumpOk ? "pump: pwm ready" : "pump: PWM unavailable");
  logger_->info(buzzerOk ? "buzzer: ready" : "buzzer: PWM unavailable");
  logger_->info(context_.vent().energiseWhenOpen() ? "vent: normally closed"
                                                   : "vent: normally open");
  logger_->info(batteryOk ? "batt: gauge ready" : "batt: no sense pin, gauge off");
  logger_->info("buttons: four keys on interrupt");

  switch (mount) {
    case Storage::Mount::Mounted:
      logger_->info("fs: littlefs mounted");
      break;
    case Storage::Mount::Formatted:
      logger_->info("fs: littlefs formatted, no filesystem found");
      break;
    case Storage::Mount::Failed:
      logger_->warn("fs: littlefs mount failed, settings will not persist");
      break;
  }
  if (mount != Storage::Mount::Failed) {
    String usage = "fs: ";
    usage += String((uint32_t)context_.storage().usedBytes());
    usage += " of ";
    usage += String((uint32_t)context_.storage().totalBytes());
    usage += " bytes used";
    logger_->info(usage.c_str());
  }
  String stored = "settings: unit=";
  stored += displayUnitInfo(context_.settings().unit()).name;
  logger_->info(stored.c_str());

  String vm = vmOk ? "vm: ready, heap " : "vm: no memory, routines off, heap ";
  vm += String((uint32_t)ESP.getFreeHeap());
  logger_->info(vm.c_str());

  link_->begin();
  // The radio is a bonus transport: a device with the preference off stays silent, and a failure is
  // a line here rather than a boot that does not come up. It comes up before the ready line so that
  // line reports the heap the firmware actually runs with.
  switch (ble_->begin()) {
    case BleLink::Start::Advertising:
      logger_->info("ble: advertising as gardener");
      break;
    case BleLink::Start::Disabled:
      logger_->info("ble: off");
      break;
    case BleLink::Start::Failed:
      logger_->warn("ble: start failed, serial only");
      break;
  }

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger_->info(ready.c_str());

  // The panel task shows the first screen when it starts: the disconnected page, because a fresh
  // boot has no controller.
  return true;
}

void App::update() {
  const uint32_t now = millis();

  // The store server applies the VM's flash and settings writes; it moves to the interface task
  // when that lands. The machine itself is serviced on the control task.
  context_.storeServer().drain();

  // The links, against the machine as the control task's last tick left it.
  link_->poll(now);
  ble_->poll(now);

  // A link that leaves while owning does not release the machine: its record stays held, the run
  // continues and the panel keeps showing that link. A reconnected controller stays a reader until
  // the local reset clears it.
  if (link_->consumeLinkLost()) {
    if (context_.ownership().ownedBy(Link::Serial)) {
      context_.ownership().drop(Link::Serial);
      logger_->warn("app: controller left, ownership held");
    } else {
      logger_->info("app: controller left");
    }
  }

  // Connection presence, published for the panel's overlay. The interface task takes over writing
  // these when it lands.
  serialConnected = serialAttached();
  bleConnected = ble_->connected();
}

}  // namespace gardener
