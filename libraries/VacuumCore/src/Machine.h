#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "Capabilities.h"
#include "Ownership.h"
#include "VacuumSystem.h"
#include "storage/Settings.h"

namespace gardener {

class Logger;

// The machine: the pressure loop and the policy around it, with no panel part named. It owns the
// parts, the ownership stack and the build's records, and it reports through the logger it was
// built with. boot() brings the parts up; the links and the tasks stay the composition's.
//
// The settings it was built with are the ones a link or the boot reads — the radio preference and
// the diagnostics one. A build with a panel hands it the panel's subclass.
//
// It is not copyable: there is one machine, and everything that needs it holds a reference.
class Machine {
 public:
  Machine(Logger& logger, Settings& settings);

  Machine(const Machine&) = delete;
  Machine& operator=(const Machine&) = delete;

  // The parts come up and say whether they answered. The links and the tasks are the
  // composition's, so a machine-alone build calls this alone.
  void boot();

  VacuumSystem& vacuum() { return vacuum_; }
  Ownership& ownership() { return ownership_; }
  Capabilities& capabilities() { return capabilities_; }
  Settings& settings() { return settings_; }
  Logger& logger() { return logger_; }
  uint32_t uptimeMs() const { return millis(); }

 private:
  Logger& logger_;
  Settings& settings_;
  VacuumSystem vacuum_;
  Ownership ownership_;
  Capabilities capabilities_;
};

}  // namespace gardener
