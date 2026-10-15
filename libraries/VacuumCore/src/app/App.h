#pragma once

#include <Adafruit_SH110X.h>
#include <Arduino.h>

#include "app/AppDisplay.h"
#include "app/SerialLink.h"
#include "app/VacuumSystem.h"
#include "core/Snapshot.h"
#include "hal/AdafruitDisplay.h"
#include "hal/Battery.h"
#include "hal/Pins.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

// Composition root. This is the only class that knows the whole graph: every other module
// is handed exactly what it uses, by reference, at construction, and nothing includes this
// header.
//
// It is also the only place that knows which display panel is fitted, and now the only thing
// that draws anything: the boot splash. Everything else on the panel is the host's, drawn over
// the serial line into the overlay, which puts the battery on every frame.
//
// The device is a serial peripheral with no local control surface, so the loop services the
// sensor, the control loop and the link, in that order. The App is also the only place that
// reports anything: the drivers return values, and the run is logged from here.

namespace gardener {

class App {
 public:
  App();

  bool begin();
  void update();

 private:
  // Fills the machine view from the modules that own the state, in place: one snapshot is
  // rebuilt per loop iteration, and the battery overlay and a reply both read that one instant.
  void refreshSnapshot();
  void logRunChanges(uint32_t nowMs);

  // The one thing the device draws for itself: what it is, and which firmware it is running,
  // before any host has said anything.
  void splash();

  // Declaration order is construction order, and several members hold references to the
  // ones above them.
  PressureSensor sensor_;
  Pump pump_;
  VentValve vent_;
  Battery battery_;
  SystemSnapshot snapshot_;
  Adafruit_SH1106G panel_;
  AdafruitDisplay display_;
  AppDisplay overlay_;
  VacuumSystem system_;
  SerialLink link_;

  RunState lastState_ = RunState::Idle;
  uint32_t ventStartMs_ = 0;
  bool displayOk_ = false;
};

}  // namespace gardener
