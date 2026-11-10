// Gardener - serial controllable vacuum pump controller.
//
// ESP32-S3 + CFSensor XGZP6847A + a vent valve. The machine is the three parts; the only way in is
// the serial link.
//
// This sketch is the device's composition root: it names the board, builds the parts, the machine
// and the link over them, and starts the two tasks that drive them.

#include <Gardener.h>
#include <Runtime.h>

namespace {

// The rig this build is wired to. The libraries take the pins, so a board variant is this block and
// nothing else.
constexpr gardener::MachinePins Board{
    .sensorAdc = 7,
    .pumpPwm = 6,
    .ventValve = 5,
};

// True while a host has the port open. With CDCOnBoot=cdc this is real USB presence; on a plain
// UART it is always true, so a lost link is never reported.
bool serialAttached() { return (bool)Serial; }

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  // The device, built in the order it is used: the log and the queue first, because the machine and
  // the link each hold one; then the parts the board wires, the machine over them, and the link.
  static gardener::SerialLogger logger(Serial);
  static gardener::runtime::RtosCommandQueue commands;
  static const gardener::runtime::MachineParts parts = gardener::runtime::buildMachineParts(Board);
  static gardener::Machine machine(logger, parts.pump, parts.sensor, parts.valve);
  static gardener::SerialLink link(Serial, serialAttached, logger, commands,
                                   gardener::SerialParts{machine});

  machine.boot();
  link.begin();

  // The control tick that services the chamber, and the interface task that answers the wire.
  gardener::runtime::spawnMachineTasks(machine, commands, &link);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger.info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
