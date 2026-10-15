#pragma once

#include <Arduino.h>

#include "core/Snapshot.h"
#include "hal/Battery.h"
#include "hal/Device.h"
#include "hal/Display.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"
#include "hosts/commands/CommandRouter.h"
#include "hosts/serial/SerialService.h"
#include "services/VacuumControl.h"
#include "ui/Renderer.h"

// Composition root. This is the only class that knows the whole graph: every other
// module is handed exactly what it uses, by reference, at construction.
//
// The device is a serial peripheral with no local control surface, so the loop services
// the sensor, the control loop and the link, in that order.

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
  void logRunChanges(uint32_t nowMs);

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
  LinkSettings link_;
  Renderer renderer_;
  CommandRouter router_;
  SerialService serial_;

  RunState lastState_ = RunState::Idle;
  uint32_t ventStartMs_ = 0;
};

}  // namespace eggplan
