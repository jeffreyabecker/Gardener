// Gardener headless - the machine alone.
//
// ESP32-S3 + CFSensor XGZP6847A + vent valve, with no panel.
//
// This sketch is the machine-alone build's composition root: it builds the two links with the
// machine's domains only, brings the machine up, and starts the control and interface tasks. Either
// link may command the machine at any time.

#include <LittleFS.h>
#include <Gardener.h>
#include <Runtime.h>

// The machine-alone build has no panel, so the radio is one of its two ways in and is not optional.
// `scripts/arduino.py` always enables it for this sketch; a build that tries to remove it stops
// here rather than linking half a composition.
#if !GARDENER_BLUETOOTH
#error "the headless build always includes the Bluetooth transport"
#endif

// Nor does it compile the panel: `scripts/arduino.py` builds it with `GARDENER_PANEL=0`, and the
// panel's sources and the Berry interpreter stay out of the image.
#if GARDENER_PANEL
#error "the headless build has no panel"
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

  // The LittleFS partition is mounted once before anything runs; a partition that will not mount is
  // formatted once, because a fresh partition is not a fault. The machine alone keeps the mount and
  // nothing reads it: its numbers are build symbols, so it has no document of its own to load.
  bool mounted = LittleFS.begin(false);
  if (!mounted) mounted = LittleFS.format() && LittleFS.begin(false);

  // The diagnostics stream is on from boot and a link switches it, the same as the full device's.
  gardener::Logger* logger = new gardener::SerialLogger(Serial);
  if (!mounted) {
    logger->warn("fs: littlefs mount failed");
  }
  gardener::Machine& machine = *new gardener::Machine(*logger, Board);

  // Nothing beyond the machine is fitted, so the capability byte stays 0: no panel, no gauge.

  // One queue into the machine: either link posts into it and the control tick drains it, so the
  // composition builds it before the links that fill it.
  auto* commands = new gardener::RtosCommandQueue();

  // The wire surface is this build's: the machine's domains and nothing else.
  gardener::SerialLink* serial =
      new gardener::SerialLink(Serial, serialAttached, *logger, *commands);
  serial->registerDomain(gardener::createVacuumDomain(machine, serial->outbox()));
  serial->registerDomain(gardener::createMachineDeviceDomain(machine, serial->outbox()));
  gardener::BleLink* ble = new gardener::BleLink(*commands);
  ble->registerDomain(gardener::createMachineBleDomain(machine));


  serial->begin();
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

  // The parts, then the two tasks that drive them.
  machine.boot();
  gardener::spawnMachineTasks(machine, *commands, serial, ble);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
