#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "app/VacuumSystem.h"
#include "hal/Battery.h"
#include "hal/ButtonPanel.h"
#include "hal/Display.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

namespace gardener {

// The hardware graph as interfaces: what App commands and the serial layer reads, with no fitted
// part named. Build.cpp owns the implementations and builds this.
class AppContext {
 public:
  AppContext(PressureSensor* sensor, Pump* pump, VentValve* vent, Battery* battery,
             ButtonPanel* buttons, Display* display, VacuumSystem* vacuum)
      : sensor_(sensor),
        pump_(pump),
        vent_(vent),
        battery_(battery),
        buttons_(buttons),
        display_(display),
        vacuum_(vacuum) {}

  PressureSensor& sensor() { return *sensor_; }
  Pump& pump() { return *pump_; }
  VentValve& vent() { return *vent_; }
  Battery& battery() { return *battery_; }
  ButtonPanel& buttons() { return *buttons_; }
  Display& display() { return *display_; }
  VacuumSystem& vacuum() { return *vacuum_; }
  uint32_t uptimeMs() const { return millis(); }

 private:
  PressureSensor* sensor_;
  Pump* pump_;
  VentValve* vent_;
  Battery* battery_;
  ButtonPanel* buttons_;
  Display* display_;
  VacuumSystem* vacuum_;
};

}  // namespace gardener
