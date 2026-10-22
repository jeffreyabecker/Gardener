// Gardener headless - the machine alone.
//
// ESP32-S3 + CFSensor XGZP6847A + vent valve, with no panel.
//
// This sketch is the machine-alone build's composition root: it builds the two links with the
// machine's domains only, brings the machine up, and starts the control and interface tasks. Its
// ownership stack is `None` alone, so an unowned machine vents and a dropped owner vents.

#include <VacuumCore.h>

// The machine-alone build has no panel, so the radio is one of its two ways in and is not optional.
// `scripts/arduino.py` always enables it for this sketch; a build that tries to remove it stops
// here rather than linking half a composition.
#if !GARDENER_BLUETOOTH
#error "the headless build always includes the Bluetooth transport"
#endif

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

  // No settings file is mounted: this build has no storage and no preferences. The diagnostics
  // stream is on from boot and a link switches it, the same as the full device's.
  gardener::Logger* logger = new gardener::SerialLogger(Serial);
  gardener::Machine& machine = *new gardener::Machine(*logger, Board);

  // Nothing beyond the machine is fitted, so the capability byte stays 0: no panel, no gauge.
  // Nothing pushes itself above `None`, so `None` is the only controller until a link claims.

  // The wire surface is this build's: the machine's domains and nothing else.
  gardener::SerialLink* serial = new gardener::SerialLink(machine, Serial, serialAttached, *logger);
  serial->registerDomain(gardener::createControlDomain(machine, *serial));
  serial->registerDomain(gardener::createVacuumDomain(machine, *serial));
  serial->registerDomain(gardener::createMachineDeviceDomain(machine, *serial));
#if GARDENER_BLUETOOTH
  gardener::BleLink* ble = new gardener::BleLink(&machine);
  ble->registerDomain(new gardener::MachineBleDomain(&machine));
#endif

  serial->begin();
#if GARDENER_BLUETOOTH
  // The radio is a bonus transport: a failure is a line here rather than a boot that does not come
  // up. It comes up before the ready line so that line reports the heap the firmware runs with.
  switch (ble->begin()) {
    case gardener::BleLink::Start::Advertising:
      logger->info("ble: advertising as gardener");
      break;
    case gardener::BleLink::Start::Failed:
      logger->warn("ble: start failed, serial only");
      break;
  }
#endif

  // The parts, then the two tasks that drive them.
  machine.boot();
#if GARDENER_BLUETOOTH
  gardener::spawnMachineTasks(machine, serial, ble);
#else
  gardener::spawnMachineTasks(machine, serial);
#endif

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
