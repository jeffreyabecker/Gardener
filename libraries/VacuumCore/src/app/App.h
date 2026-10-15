#pragma once

#include <Arduino.h>

#include "app/SerialLink.h"
#include "app/SnapshotBuilder.h"
#include "app/StatusPage.h"
#include "app/VacuumControl.h"
#include "core/Snapshot.h"
#include "hal/Battery.h"
#include "hal/Display.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

// Composition root. This is the only class that knows the whole graph: every other module
// is handed exactly what it uses, by reference, at construction, and nothing includes this
// header.
//
// The device is a serial peripheral with no local control surface, so the loop services the
// sensor, the control loop and the link, in that order. The App is also the only place that
// reports anything: the drivers return values, and the run is logged from here.

namespace eggplan {

class App {
 public:
  App();

  bool begin();
  void update();

 private:
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
  SystemSnapshot snapshot_;
  StatusPage page_;
  SerialLink link_;

  RunState lastState_ = RunState::Idle;
  uint32_t ventStartMs_ = 0;
};

}  // namespace eggplan
