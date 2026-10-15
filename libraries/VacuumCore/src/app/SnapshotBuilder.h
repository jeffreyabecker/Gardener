#pragma once

#include "core/Snapshot.h"

namespace eggplan {

class Battery;
class PressureSensor;
class Pump;
class VacuumControl;

// Fills the machine view from the modules that own the state, in place: one snapshot is
// rebuilt per loop iteration, and the page and a reply both read that one instant.
// Free function: there is nothing to keep between calls.

void buildSnapshot(const Pump& pump, const PressureSensor& sensor, const VacuumControl& control,
                   const Battery& battery, SystemSnapshot& out);

}  // namespace eggplan
