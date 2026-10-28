// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery gauge.
//
// This sketch is the full device's composition root: it builds the two links, registers the
// machine's and the panel's domains on them, brings the machine and the panel up, and starts the
// three tasks. A build with no panel does the same with two tasks and no panel domain.

#include <LittleFS.h>
#include <Gardener.h>
#include <Runtime.h>

namespace {

// The rig this build is wired to. The libraries take the pins, so a board variant is this block and
// nothing else.
constexpr gardener::PanelPins Board{
    .machine = {.sensorAdc = 7, .pumpPwm = 6, .ventValve = 5},
    .i2cSda = 17,
    .i2cScl = 18,
    .i2cHz = 400000,
    .buzzerPwm = 12,
    .batteryAdc = 4,
    .batteryDivider = 2.0f,
    .keyLatch = 10,
    .keyClock = 11,
    .keyData = 1,
};

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a plain
// UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

void setup() {
  Serial.begin(115200);
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

  // The diagnostics stream is not a setting: it is on from boot, a link switches it with `device
  // set log`, and a reset returns it to on. The panel reads its documents and the interpreter's
  // files through the one store.
  auto* files = new gardener::FileLibrary(LittleFS);
  auto* logger = new gardener::SerialLogger(Serial);

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

  // One queue into the machine: the panel, either link and the control tick all reach it, so the
  // composition builds it before any of them.
  auto* commands = new gardener::RtosCommandQueue();

  // The full device's graph: the machine, with the panel arranged around it.
  gardener::Panel& context =
      gardener::buildPanelRoot(*logger, LittleFS, *files, Board, *commands);

  // The wire surface is this build's to compose: the machine's domains and the panel's, both on the
  // one serial link and the one BLE link. The panel is a machine, so the same object serves both
  // halves.
  gardener::SerialLink* serial =
      new gardener::SerialLink(Serial, serialAttached, *logger, *commands);
  serial->registerDomain(gardener::createVacuumDomain(context, serial->outbox()));
  serial->registerDomain(gardener::createDeviceDomain(context, serial->outbox()));
  serial->registerDomain(gardener::createBuzzerDomain(context, serial->outbox()));
  serial->registerDomain(gardener::createFsDomain(context, serial->outbox()));
#if GARDENER_BLUETOOTH
  gardener::BleLink* ble = new gardener::BleLink(*commands);
  ble->registerDomain(gardener::createMachineBleDomain(context));
  ble->registerDomain(gardener::createPanelBleDomain(context));
#endif

  serial->begin();
#if GARDENER_BLUETOOTH
  // The radio is a bonus transport: a failure is a line here rather than a boot that does not come
  // up. It comes up before the ready line so that line reports the heap the firmware actually runs
  // with.
  switch (ble->begin()) {
    case gardener::BleLink::Start::Advertising:
      logger->info("ble: advertising as gardener");
      break;
    case gardener::BleLink::Start::Failed:
      logger->warn("ble: start failed, serial only");
      break;
  }
#endif

  // The parts, then the tasks that drive them.
  context.boot();

#if GARDENER_BLUETOOTH
  gardener::spawnMachineTasks(context, *commands, serial, ble);
#else
  gardener::spawnMachineTasks(context, *commands, serial);
#endif
  gardener::spawnPanelTask(context);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
