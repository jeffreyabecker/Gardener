#pragma once

#include <Arduino.h>

#include "core/ReplyQueue.h"
#include "core/Snapshot.h"
#include "hal/Battery.h"
#include "hal/Device.h"
#include "hal/Display.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"
#include "hosts/ble/BleService.h"
#include "hosts/commands/CommandRouter.h"
#include "services/VacuumControl.h"
#include "ui/Renderer.h"

// Composition root. This is the only class that knows the whole graph: every other
// module is handed exactly what it uses, by reference, at construction.
//
// The device is a BLE peripheral with no local control surface, so the loop only has to
// service the sensor, the control loop and the transport.

namespace eggplan {

class App {
 public:
  App();

  bool begin();
  void update();

 private:
  static PumpConfig pumpConfig();
  static PidConfig pidConfig();
  static VentValveConfig ventConfig();
  static BatteryConfig batteryConfig();
  void refreshSnapshot();

  // Declaration order is construction order, and several members hold references to the
  // ones above them.
  PressureSensor sensor_;
  Pump pump_;
  VentValve vent_;
  Battery battery_;
  Display display_;
  VacuumControl control_;
  DeviceInfo device_;
  SystemSnapshot snapshot_;
  TelemetrySettings telemetry_;
  Renderer renderer_;
  ReplyQueue replies_;
  ReplyQueue inbound_;
  CommandRouter router_;
  BleService ble_;
};

}  // namespace eggplan
