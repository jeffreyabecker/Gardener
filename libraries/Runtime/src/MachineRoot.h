#pragma once

#include "machine/Machine.h"

namespace gardener {

// The three parts the board wires, in the order the vacuum system commands them.
struct MachineParts {
  Pump* pump;
  PressureSensor* sensor;
  VentValve* valve;
};

// Builds the fitted pump, sensor and valve from the board's pins, choosing the valve from the
// build's VALVE_NORMALLY_OPEN: normally open keeps the fail-safe (de-energised vents), normally
// closed inverts it.
MachineParts buildMachineParts(const MachinePins& pins);

}  // namespace gardener
