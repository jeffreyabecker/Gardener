// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery gauge.
//
// This sketch is the full device's composition root: it builds the two links, registers the
// machine's and the panel's domains on them, brings the machine and the panel up, and starts the
// three tasks. A build with no panel does the same with two tasks and no panel domain.

#include <LittleFS.h>
#include <PanelLink.h>

namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a plain
// UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

void setup() {
  Serial.begin(gardener::cfg::SERIAL_BAUD);
  delay(200);

  // The device's files are on the LittleFS partition the build reserves. It is mounted once here,
  // before anything reads one; a partition that will not mount is formatted once, because a fresh
  // partition is not a fault.
  bool mounted = LittleFS.begin(false);
  bool formatted = false;
  if (!mounted) {
    formatted = LittleFS.format() && LittleFS.begin(false);
    mounted = formatted;
  }

  // The settings pick the logger, and the machine is built with the logger, so the preferences are
  // read before the panel is.
  auto* settings = new gardener::Settings();
  settings->begin();

  gardener::Logger* logger =
      settings->logEnabled()
          ? static_cast<gardener::Logger*>(new gardener::StreamLogger(Serial))
          : static_cast<gardener::Logger*>(new gardener::NilLogger());

  if (mounted) {
    logger->info(formatted ? "fs: littlefs formatted, no filesystem found"
                           : "fs: littlefs mounted");
    String usage = "fs: ";
    usage += String((uint32_t)LittleFS.usedBytes());
    usage += " of ";
    usage += String((uint32_t)LittleFS.totalBytes());
    usage += " bytes used";
    logger->info(usage.c_str());
  } else {
    logger->warn("fs: littlefs mount failed, settings will not persist");
  }

  // The full device's graph: the machine, with the panel arranged around it.
  gardener::Panel& context = gardener::buildPanelRoot(*logger, *settings);
  context.config().bluetooth = settings->bluetoothEnabled();
  context.config().diagnostics = settings->logEnabled();

  // The wire surface is this build's to compose: the machine's domains and the panel's, both on the
  // one serial link and the one BLE link. The panel is a machine, so the same object serves both
  // halves.
  gardener::SerialLink* serial = new gardener::SerialLink(context, Serial, serialAttached, *logger);
  serial->registerDomain(gardener::createControlDomain(context, *serial));
  serial->registerDomain(gardener::createVacuumDomain(context, *serial));
  serial->registerDomain(gardener::createDeviceDomain(context, *serial));
  serial->registerDomain(gardener::createBuzzerDomain(context, *serial));
  serial->registerDomain(gardener::createFsDomain(context, *serial));
  gardener::BleLink* ble = new gardener::BleLink(&context);
  ble->registerDomain(new gardener::MachineBleDomain(&context));
  ble->registerDomain(new gardener::PanelBleDomain(&context));

  serial->begin();
  // The radio is a bonus transport: a device with the preference off stays silent, and a failure is
  // a line here rather than a boot that does not come up. It comes up before the ready line so that
  // line reports the heap the firmware actually runs with.
  switch (ble->begin()) {
    case gardener::BleLink::Start::Advertising:
      logger->info("ble: advertising as gardener");
      break;
    case gardener::BleLink::Start::Disabled:
      logger->info("ble: off");
      break;
    case gardener::BleLink::Start::Failed:
      logger->warn("ble: start failed, serial only");
      break;
  }

  // The parts, then the tasks that drive them.
  context.boot();

  auto* connected = new gardener::ConnectedUI(context);
  auto* disconnected = new gardener::DisconnectedUI(context);
  gardener::spawnMachineTasks(context, serial, ble);
  gardener::spawnPanelTask(context, connected, disconnected);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
