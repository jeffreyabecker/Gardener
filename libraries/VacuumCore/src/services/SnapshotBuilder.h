#pragma once

#include "core/Snapshot.h"

namespace eggplan {

class PressureSensor;
class Pump;
class VacuumControl;

// Fills the machine view from the modules that own the state. Free functions: there is
// nothing to keep between calls.

SystemSnapshot buildSnapshot(const Pump& pump, const PressureSensor& sensor,
                            const VacuumControl& control, const DeviceInfo& device);

}  // namespace eggplan
