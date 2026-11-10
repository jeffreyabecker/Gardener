// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + SH1106 OLED, with a vent valve and a 1S Li-ion battery gauge.
//
// This sketch is the device's composition root. PanelRoot builds and owns the parts, and this file
// names the board, hands each link the parts it serves, brings the parts up and starts the three
// tasks.

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
    // The panel's keys in the order the register presents them, Up first.
    .keyMask = {1, 2, 4, 8, 16, 32, 64, 128},
};

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a plain
// UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  // The whole device, owned in one place; its constructor builds the graph and begin() brings it up.
  static gardener::PanelRoot root(Board);

  // The wire surface is this build's to compose: the serial link serves the machine's commands, and
  // it is the only way a controller reaches the device.
  gardener::SerialLink* serial =
      new gardener::SerialLink(Serial, serialAttached, root.logger(), root.commands(),
                               gardener::SerialParts{root.machine()});

  serial->begin();

  // The parts, then the tasks that drive them. A glass that does not answer is a panic and a reset
  // rather than a device that comes up blind.
  if (!root.begin()) {
    delay(50);
    abort();
  }

  gardener::runtime::spawnMachineTasks(root.machine(), root.commands(), serial, root.state());
  gardener::runtime::spawnPanelTask(root.buttons(), root.routines().presses(), root.commands(),
                                    root.logger(), root.battery(), root.buzzer(), root.routines(),
                                    root.machine(), root.state());

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  root.logger().info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
