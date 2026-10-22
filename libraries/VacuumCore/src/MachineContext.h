#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "Capabilities.h"
#include "MachineConfig.h"
#include "Ownership.h"
#include "VacuumSystem.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hal/VentValve.h"

namespace gardener {

// The machine's graph: the pressure loop and the policy around it, with no panel part named. The
// composition root builds it; the machine's code — the control and interface beats and the wire
// domains — reads it.
class MachineContext {
 public:
  MachineContext(PressureSensor* sensor, Pump* pump, VentValve* vent, VacuumSystem* vacuum,
                 Ownership* ownership, Capabilities* capabilities)
      : sensor_(sensor),
        pump_(pump),
        vent_(vent),
        vacuum_(vacuum),
        ownership_(ownership),
        capabilities_(capabilities) {}

  PressureSensor& sensor() { return *sensor_; }
  Pump& pump() { return *pump_; }
  VentValve& vent() { return *vent_; }
  VacuumSystem& vacuum() { return *vacuum_; }
  Ownership& ownership() { return *ownership_; }
  Capabilities& capabilities() { return *capabilities_; }
  MachineConfig& config() { return config_; }
  uint32_t uptimeMs() const { return millis(); }

 private:
  PressureSensor* sensor_;
  Pump* pump_;
  VentValve* vent_;
  VacuumSystem* vacuum_;
  Ownership* ownership_;
  Capabilities* capabilities_;
  MachineConfig config_;
};

}  // namespace gardener
