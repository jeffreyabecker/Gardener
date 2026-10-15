#pragma once

#include "core/Snapshot.h"

namespace eggplan {

class Battery;
class PressureSensor;
class Pump;
class VacuumControl;

// Fills the machine view from the modules that own the state. Free function: there is
// nothing to keep between calls.

SystemSnapshot buildSnapshot(const Pump& pump, const PressureSensor& sensor,
                            const VacuumControl& control, const Battery& battery);

}  // namespace eggplan
