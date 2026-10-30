// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery gauge.
//
// This sketch is the device's composition root: it builds the two links, registers the machine's and
// the panel's domains on them, brings the machine and the panel up, and starts the three tasks.

#include <LittleFS.h>
#include <Gardener.h>
#include <Runtime.h>

namespace {

// The rig this build is wired to. The libraries take the pins, so a board variant is this block and
// nothing else.
constexpr gardener::panel::PanelPins Board{
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

// The board's own facts, which the `device` reply reports. The heap is read when a controller asks,
// so a host sees the firmware's figure rather than the one it booted with.
const gardener::BoardFacts BoardInfo{
    [] { return ESP.getChipModel(); },
    [] { return (uint32_t)ESP.getFreeHeap(); },
};

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

  // The panel's view of the device: each task publishes the slice it owns into this and whatever
  // draws reads it, so the composition owns it and hands a reference to everything that touches it.
  static gardener::SystemState panelState;

  // The full device's graph: the machine, with the panel arranged around it.
  gardener::panel::Panel& context =
      gardener::buildPanelRoot(*logger, *files, Board, *commands, panelState);

  // The wire surface is this build's to compose: the machine's domains and the panel's, both on the
  // one serial link and the one BLE link. The machine and the panel's parts are the composition's,
  // so the same objects serve both halves.
  gardener::SerialLink* serial =
      new gardener::SerialLink(Serial, serialAttached, *logger, *commands);
  serial->registerDomain(gardener::createVacuumDomain(context.machine(), serial->outbox()));
  serial->registerDomain(
      gardener::createDeviceDomain(context.machine(), serial->outbox(), BoardInfo,
                                   context.battery(), context.settings()));
  serial->registerDomain(gardener::createBuzzerDomain(context.buzzer(), serial->outbox()));
  serial->registerDomain(gardener::createFsDomain(serial->outbox(), LittleFS));
  gardener::BleLink* ble = new gardener::BleLink(*commands);
  ble->registerDomain(gardener::createMachineBleDomain(context.machine()));
  ble->registerDomain(gardener::createPanelBleDomain(context.settings(), context.battery()));

  serial->begin();
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

  // The parts, then the tasks that drive them.
  gardener::bootPanel(context, Board);

  gardener::spawnMachineTasks(context.machine(), *commands, serial, ble, panelState);
  gardener::spawnPanelTask(context.buttons(), context.routines().presses(), context.outbox(),
                           context.logger(), context.battery(), context.buzzer(),
                           context.routines(), context.machine(), panelState);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger->info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
