#pragma once

#include "machine/Machine.h"

namespace gardener {

class Logger;

// The three parts the board wires, in the order the vacuum system commands them.
struct MachineParts {
  Pump* pump;
  PressureSensor* sensor;
  VentValve* valve;
};

// Builds the fitted pump, sensor and valve from the board's pins, choosing the valve from the
// build's VALVE_NORMALLY_OPEN: normally open keeps the fail-safe (de-energised vents), normally
// closed inverts it. Both compositions use it — the machine-alone build hands the three to a
// Machine, and the panel build arranges the panel around the same three.
MachineParts buildMachineParts(const MachinePins& pins);

// The machine-alone composition: the parts above, handed to the machine that commands them.
Machine& buildMachineRoot(Logger& logger, const MachinePins& pins);

}  // namespace gardener
