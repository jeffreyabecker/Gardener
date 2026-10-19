#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "app/BuzzerPlayer.h"
#include "app/Overlay.h"
#include "app/Settings.h"
#include "app/Storage.h"
#include "app/VacuumSystem.h"
#include "app/disconnected-ui/Routines.h"
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
             ButtonPanel* buttons, BuzzerPlayer* buzzer, Overlay* overlay, VacuumSystem* vacuum,
             Storage* storage, Settings* settings, Routines* routines)
      : sensor_(sensor),
        pump_(pump),
        vent_(vent),
        battery_(battery),
        buttons_(buttons),
        buzzer_(buzzer),
        overlay_(overlay),
        vacuum_(vacuum),
        storage_(storage),
        settings_(settings),
        routines_(routines) {}

  PressureSensor& sensor() { return *sensor_; }
  Pump& pump() { return *pump_; }
  VentValve& vent() { return *vent_; }
  Battery& battery() { return *battery_; }
  ButtonPanel& buttons() { return *buttons_; }
  BuzzerPlayer& buzzer() { return *buzzer_; }
  // The overlay is the display — every drawing verb goes through it — so both views are the same
  // object. The concrete one is here because the links are the loop's to report.
  Display& display() { return *overlay_; }
  Overlay& overlay() { return *overlay_; }
  VacuumSystem& vacuum() { return *vacuum_; }
  Storage& storage() { return *storage_; }
  Settings& settings() { return *settings_; }
  Routines& routines() { return *routines_; }
  uint32_t uptimeMs() const { return millis(); }

 private:
  PressureSensor* sensor_;
  Pump* pump_;
  VentValve* vent_;
  Battery* battery_;
  ButtonPanel* buttons_;
  BuzzerPlayer* buzzer_;
  Overlay* overlay_;
  VacuumSystem* vacuum_;
  Storage* storage_;
  Settings* settings_;
  Routines* routines_;
};

}  // namespace gardener
