// Gardener headless - the machine alone.
//
// ESP32-S3 + CFSensor XGZP6847A + vent valve, with no panel.
//
// This sketch is the machine-alone build's composition root: it builds the two links with the
// machine's domains only, brings the machine up, and starts the control and interface tasks. Its
// ownership stack is `None` alone, so an unowned machine vents and a dropped owner vents.

#include <VacuumCore.h>

namespace {

// The rig this build is wired to: the machine's three parts, and nothing else.
constexpr gardener::MachinePins Board{.sensorAdc = 7, .pumpPwm = 6, .ventValve = 5};

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a plain
// UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  // No settings file is mounted, so every preference is its default and the diagnostics stream is
  // the build's — the only way to see what the device is doing.
  gardener::Logger* logger = new gardener::StreamLogger(Serial);
  gardener::Settings& settings = *new gardener::Settings(nullptr);
  gardener::Machine& machine = *new gardener::Machine(*logger, settings, Board);

  // Nothing beyond the machine is fitted, so the capability byte stays 0: no panel, no gauge.
  // Nothing pushes itself above `None`, so `None` is the only controller until a link claims.

  // The wire surface is this build's: the machine's domains and nothing else.
  gardener::SerialLink* serial = new gardener::SerialLink(machine, Serial, serialAttached, *logger);
  serial->registerDomain(gardener::createControlDomain(machine, *serial));
  serial->registerDomain(gardener::createVacuumDomain(machine, *serial));
  serial->registerDomain(gardener::createMachineDeviceDomain(machine, *serial));
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
  machine.boot();
  gardener::spawnMachineTasks(machine, serial, ble);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
