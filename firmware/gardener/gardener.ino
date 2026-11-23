// Gardener - WiFi and Bluetooth controllable vacuum pump controller.
//
// ESP32-C3 Super Mini + CFSensor XGZP6847A + a vent valve. The machine is the three parts; the ways
// in are the web page and the Bluetooth LE link.
//
// This sketch is the device's composition root: it names the board, builds the parts, the machine
// and the links over them, and starts the tasks that drive them.

#include <Gardener.h>

#include "Runtime.h"

namespace {

// The board this build is wired to. sketch.yaml names the pins and the wrapper writes them into
// options.h, so a board variant is that block and nothing else: the libraries take the pins.
#if !defined(GARDENER_PIN_SENSOR_ADC) || !defined(GARDENER_PIN_PUMP) ||                            \
    !defined(GARDENER_PIN_VENT_VALVE)
#error "sketch.yaml must name PIN_SENSOR_ADC, PIN_PUMP and PIN_VENT_VALVE under defines:"
#endif
constexpr gardener::MachinePins Board{
    .sensorAdc = GARDENER_PIN_SENSOR_ADC,
    .pump = GARDENER_PIN_PUMP,
    .ventValve = GARDENER_PIN_VENT_VALVE,
};

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  // The device, built in the order it is used: the log and the queue first, because the machine and
  // the control each hold one; then the parts the board wires, the machine over them, the control
  // that any front end asks, and the two radio links over that control.
  static gardener::SerialLogger logger(Serial);
  static gardener::runtime::RtosCommandQueue commands;
  static const gardener::runtime::MachineParts parts = gardener::runtime::buildMachineParts(Board);
  static gardener::Machine machine(logger, parts.pump, parts.sensor, parts.valve);
  static gardener::SystemControl control(machine.vacuum(), commands);
  static gardener::runtime::WebLink web(control, logger);
  static gardener::runtime::BleLink bluetooth(control, logger);

  machine.boot();
  web.begin();
  // A Bluetooth stack that will not start is a lost feature rather than an unbootable board.
  bluetooth.begin();

  // The control tick that services the chamber, and the interface task that answers the links.
  gardener::runtime::spawnMachineTasks(machine, commands, &bluetooth);

  String ready = "app: ready, heap ";
  ready += String((uint32_t)ESP.getFreeHeap());
  logger.info(ready.c_str());
}

void loop() { vTaskDelete(NULL); }
