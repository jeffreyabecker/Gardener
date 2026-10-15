#pragma once

#include "core/Snapshot.h"

namespace eggplan {

class PressureSensor;
class ProgramStore;
class Pump;
class VacuumControl;

// Fills the machine view from the modules that own the state. Free functions: there
// is nothing to keep between calls, and taking the runtime would be a services ->
// hosts include, so App fills the script fields afterwards.

SystemSnapshot buildSnapshot(const Pump& pump, const PressureSensor& sensor,
                           const VacuumControl& control, const ProgramStore& store,
                           const DeviceInfo& device);

}  // namespace eggplan
