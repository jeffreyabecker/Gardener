// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery gauge.
//
// This sketch is the device's composition root. PanelRoot builds and owns the parts, and this file
// names the board, registers the machine's and the panel's domains on the two links, brings the
// parts up and starts the three tasks.

#include <LittleFS.h>
#include <Gardener.h>
#include <Runtime.h>

#include "PanelRoot.h"

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

  // The whole device, owned in one place; its constructor builds the graph and begin() brings it up.
  static gardener::PanelRoot root(Board);

  // The wire surface is this build's to compose: the machine's domains and the panel's, both on the
  // one serial link and the one BLE link.
  gardener::SerialLink* serial =
      new gardener::SerialLink(Serial, serialAttached, root.logger(), root.commands());
  serial->registerDomain(gardener::createVacuumDomain(root.machine(), serial->outbox()));
  serial->registerDomain(
      gardener::createDeviceDomain(root.machine(), serial->outbox(), BoardInfo,
                                   root.battery(), root.settings()));
  serial->registerDomain(gardener::createBuzzerDomain(root.buzzer(), serial->outbox()));
  serial->registerDomain(gardener::createFsDomain(serial->outbox(), LittleFS));
  gardener::BleLink* ble = new gardener::BleLink(root.commands());
  ble->registerDomain(gardener::createMachineBleDomain(root.machine()));
  ble->registerDomain(gardener::createPanelBleDomain(root.settings(), root.battery()));

  serial->begin();
  // The radio is a bonus transport: a failure is a line here rather than a boot that does not come
  // up. It comes up before the ready line so that line reports the heap the firmware actually runs
  // with.
  switch (ble->begin()) {
    case gardener::BleLink::Start::Advertising:
      root.logger().info("ble: advertising as gardener");
      break;
    case gardener::BleLink::Start::Failed:
      root.logger().warn("ble: start failed, serial only");
      break;
  }

  // The parts, then the tasks that drive them. A glass that does not answer is a panic and a reset
  // rather than a device that comes up blind.
  if (!root.begin()) {
    delay(50);
    abort();
  }

  gardener::spawnMachineTasks(root.machine(), root.commands(), serial, ble, root.state());
  gardener::spawnPanelTask(root.buttons(), root.routines().presses(), root.outbox(),
                           root.logger(), root.battery(), root.buzzer(), root.routines(),
                           root.machine(), root.state());

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  root.logger().info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
