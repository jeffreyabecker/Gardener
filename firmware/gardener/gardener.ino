// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery gauge.
//
// This sketch is the full device's composition root: it builds the two links, registers the
// machine's and the panel's domains on them, brings the machine and the panel up, and starts the
// three tasks. A build with no panel does the same with two tasks and no panel domain.

#include <PanelLink.h>

namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a plain
// UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

gardener::PanelContext& context = gardener::buildPanelRoot();

void setup() {
  Serial.begin(gardener::cfg::SERIAL_BAUD);
  delay(200);

  // The files and the persisted knobs come up first. Storage and settings are the panel's, but the
  // knobs they persist — the radio and the diagnostics — are the machine's, so they are read into
  // the machine's config and the logger is picked before either boot.
  gardener::Storage::Mount mount = context.storage().begin();
  context.settings().begin();
  context.config().bluetooth = context.settings().bluetoothEnabled();
  context.config().diagnostics = context.settings().logEnabled();
  gardener::Logger* logger =
      context.config().diagnostics
          ? static_cast<gardener::Logger*>(new gardener::StreamLogger(Serial))
          : static_cast<gardener::Logger*>(new gardener::NilLogger());

  // The wire surface is this build's to compose: the machine's domains and the panel's, both on the
  // one serial link and the one BLE link. The panel context is a machine context, so the same object
  // serves both halves.
  gardener::SerialLink* serial = new gardener::SerialLink(Serial, serialAttached, *logger);
  serial->registerDomain(gardener::createControlDomain(context));
  serial->registerDomain(gardener::createVacuumDomain(context));
  serial->registerDomain(gardener::createDeviceDomain(context));
  serial->registerDomain(gardener::createBuzzerDomain(context));
  serial->registerDomain(gardener::createFsDomain(context));
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
  gardener::machineBoot(context, *logger);
  gardener::panelBoot(context, *logger, mount);

  auto* connected = new gardener::ConnectedUI(context);
  auto* disconnected = new gardener::DisconnectedUI(context);
  gardener::spawnMachineTasks(context, *logger, serial, ble);
  gardener::spawnPanelTask(context, *logger, connected, disconnected);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
