#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "Capabilities.h"
#include "MachineConfig.h"
#include "Ownership.h"
#include "VacuumSystem.h"

namespace gardener {

class Logger;

// The machine: the pressure loop and the policy around it, with no panel part named. It owns the
// parts, the ownership stack and the build's records, and it reports through the logger it was
// built with. boot() brings the parts up; the links and the tasks stay the composition's.
//
// It is not copyable: there is one machine, and everything that needs it holds a reference.
class Machine {
 public:
  explicit Machine(Logger& logger);

  Machine(const Machine&) = delete;
  Machine& operator=(const Machine&) = delete;

  // The parts come up and say whether they answered. The links and the tasks are the
  // composition's, so a machine-alone build calls this alone.
  void boot();

  VacuumSystem& vacuum() { return vacuum_; }
  Ownership& ownership() { return ownership_; }
  Capabilities& capabilities() { return capabilities_; }
  MachineConfig& config() { return config_; }
  Logger& logger() { return logger_; }
  uint32_t uptimeMs() const { return millis(); }

 private:
  Logger& logger_;
  VacuumSystem vacuum_;
  Ownership ownership_;
  Capabilities capabilities_;
  MachineConfig config_;
};

}  // namespace gardener
