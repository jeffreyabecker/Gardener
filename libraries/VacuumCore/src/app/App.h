#pragma once

#include <Adafruit_SH110X.h>
#include <Arduino.h>

#include "app/SerialLink.h"
#include "app/StatusPage.h"
#include "app/VacuumControl.h"
#include "app/VacuumSystem.h"
#include "core/Snapshot.h"
#include "hal/Battery.h"
#include "hal/Pins.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

// Composition root. This is the only class that knows the whole graph: every other module
// is handed exactly what it uses, by reference, at construction, and nothing includes this
// header.
//
// It is also the only place that knows which display panel is fitted: the page draws through
// Adafruit_GFX, and the panel object lives here.
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
  // Fills the machine view from the modules that own the state, in place: one snapshot is
  // rebuilt per loop iteration, and the page and a reply both read that one instant.
  void refreshSnapshot();
  void logRunChanges(uint32_t nowMs);

  // Declaration order is construction order, and several members hold references to the
  // ones above them.
  PressureSensor sensor_;
  Pump pump_;
  VentValve vent_;
  Battery battery_;
  Adafruit_SH1106G panel_;
  VacuumSystem system_;
  VacuumControl control_;
  SystemSnapshot snapshot_;
  StatusPage page_;
  SerialLink link_;

  RunState lastState_ = RunState::Idle;
  uint32_t ventStartMs_ = 0;
  bool displayOk_ = false;
};

}  // namespace eggplan
