#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "Capabilities.h"
#include "MachineConfig.h"
#include "Ownership.h"
#include "VacuumSystem.h"

namespace gardener {

// The machine's graph: the pressure loop and the policy around it, with no panel part named. The
// composition root builds it; the machine's code — the control and interface beats and the wire
// domains — reads it. The parts the loop commands are reached through it, so their driver reads live
// on the vacuum system and not again here.
class MachineContext {
 public:
  MachineContext(VacuumSystem* vacuum, Ownership* ownership, Capabilities* capabilities)
      : vacuum_(vacuum), ownership_(ownership), capabilities_(capabilities) {}

  VacuumSystem& vacuum() { return *vacuum_; }
  Ownership& ownership() { return *ownership_; }
  Capabilities& capabilities() { return *capabilities_; }
  MachineConfig& config() { return config_; }
  uint32_t uptimeMs() const { return millis(); }

 private:
  VacuumSystem* vacuum_;
  Ownership* ownership_;
  Capabilities* capabilities_;
  MachineConfig config_;
};

}  // namespace gardener
