// Gardener headless - the machine alone.
//
// ESP32-S3 + CFSensor XGZP6847A + vent valve, with no panel.
//
// This sketch is the machine-alone build's composition root: it builds the two links with the
// machine's domains only, brings the machine up, and starts the control and interface tasks. Its
// ownership stack is `None` alone, so an unowned machine vents and a dropped owner vents.

#include <VacuumCore.h>

namespace {

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a plain
// UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

gardener::MachineContext& machine = gardener::buildMachineRoot();

void setup() {
  Serial.begin(gardener::cfg::SERIAL_BAUD);
  delay(200);

  gardener::Logger* logger = new gardener::StreamLogger(Serial);

  // The wire surface is this build's: the machine's domains and nothing else. There is no `device`
  // domain yet, so nothing answers `device get`.
  gardener::SerialLink* serial = new gardener::SerialLink(Serial, serialAttached, *logger);
  serial->registerDomain(gardener::createControlDomain(machine));
  serial->registerDomain(gardener::createVacuumDomain(machine));
  gardener::BleLink* ble = new gardener::BleLink(&machine);
  ble->registerDomain(new gardener::MachineBleDomain(&machine));

  serial->begin();
  // The radio is a bonus transport: a failure is a line here rather than a boot that does not come
  // up. It comes up before the ready line so that line reports the heap the firmware runs with.
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

  // The parts, then the two tasks that drive them.
  gardener::machineBoot(machine, *logger);
  gardener::spawnMachineTasks(machine, *logger, serial, ble);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
