#pragma once

#include <Adafruit_GrayOLED.h>
#include <Arduino.h>

#include "app/Overlay.h"
#include "app/serial/SerialLink.h"
#include "app/VacuumSystem.h"
#include "core/Snapshot.h"
#include "hal/Battery.h"
#include "hal/impl/AdafruitDisplay.h"
#include "hal/impl/MprlsSensor.h"
#include "hal/impl/PwmPump.h"
#include "hal/impl/SolenoidValve.h"

// Composition root. This is the only class that knows the whole graph: every other module
// is handed exactly what it uses, by reference, at construction, and nothing includes this
// header.
//
// It is also the only thing that draws anything for itself: the boot splash. Everything else on
// the panel is the host's, drawn over the serial line into the overlay, which puts the battery on
// every frame. Which panel is fitted is a detail that may change; App holds it as a GrayOLED.
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
  // The one place the fitted panel is named: it new's the driver, brings it up on the I2C bus, and
  // hands it back as the drawing base. A panel that does not answer is fatal, so this never
  // returns null.
  Adafruit_GrayOLED* attachPanel();

  // Fills the machine view from the modules that own the state, in place: one snapshot is
  // rebuilt per loop iteration, and the battery overlay and a reply both read that one instant.
  void refreshSnapshot();
  void logRunChanges(uint32_t nowMs);

  // The one thing the device draws for itself: what it is, and which firmware it is running,
  // before any host has said anything.
  void splash();

  // Declaration order is construction order, and several members hold references to the
  // ones above them.
  MprlsSensor sensor_;
  PwmPump pump_;
  // The one place the fitted valve is named: NormallyOpenValve is the fail-safe part (energise to
  // close), NormallyClosedValve is for holding vacuum through a power loss (energise to open).
  NormallyOpenValve vent_;
  Battery battery_;
  SystemSnapshot snapshot_;
  Adafruit_GrayOLED* panel_ = nullptr;
  AdafruitDisplay display_;
  Overlay overlay_;
  VacuumSystem system_;
  SerialLink link_;

  RunState lastState_ = RunState::Idle;
  uint32_t ventStartMs_ = 0;
};

}  // namespace gardener
