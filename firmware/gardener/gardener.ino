// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery gauge.
//
// This sketch is the full device's composition root: it builds the two links, registers the
// machine's and the panel's domains on them, brings the machine and the panel up, and starts the
// three tasks. A build with no panel does the same with two tasks and no panel domain.

#include <GardenerPanel.h>

namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a plain
// UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

gardener::AppContext context = gardener::buildAppContext();

void setup() {
  Serial.begin(gardener::cfg::SERIAL_BAUD);
  delay(200);

  // The files and the persisted knobs come up first. Storage and settings are the panel's, but the
  // knobs they persist — the radio and the diagnostics — are the machine's, so they are read into
  // the machine's config and the logger is picked before either boot.
  gardener::Storage::Mount mount = context.panel().storage().begin();
  context.panel().settings().begin();
  context.machine().config().bluetooth = context.panel().settings().bluetoothEnabled();
  context.machine().config().diagnostics = context.panel().settings().logEnabled();
  gardener::Logger* logger =
      context.machine().config().diagnostics
          ? static_cast<gardener::Logger*>(new gardener::StreamLogger(Serial))
          : static_cast<gardener::Logger*>(new gardener::NilLogger());

  // The wire surface is this build's to compose: the machine's domains on the machine context, the
  // panel's on the panel context, both on the one serial link and the one BLE link.
  gardener::SerialLink* serial = new gardener::SerialLink(Serial, serialAttached, *logger);
  serial->registerDomain(gardener::createControlDomain(context.machine()));
  serial->registerDomain(gardener::createVacuumDomain(context.machine()));
  serial->registerDomain(gardener::createDeviceDomain(context.panel()));
  serial->registerDomain(gardener::createBuzzerDomain(context.panel()));
  serial->registerDomain(gardener::createFsDomain(context.panel()));
  gardener::BleLink* ble = new gardener::BleLink(&context.machine());
  ble->registerDomain(new gardener::MachineBleDomain(&context.machine()));
  ble->registerDomain(new gardener::PanelBleDomain(&context.panel()));

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
  gardener::machineBoot(context.machine(), *logger);
  gardener::panelBoot(context.panel(), *logger, mount);

  auto* connected = new gardener::ConnectedUI(context.panel());
  auto* disconnected = new gardener::DisconnectedUI(context.panel());
  gardener::spawnMachineTasks(context.machine(), *logger, serial, ble);
  gardener::spawnPanelTask(context.panel(), *logger, connected, disconnected);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
