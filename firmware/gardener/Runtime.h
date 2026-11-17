#pragma once

// Umbrella header for the device's own composition, beside the sketch. It names what the board
// supplies — the builder of the pump, the sensor and the valve from the pins — and the control task
// that steps the tick and the interface task that polls the link.

#include "MachineRoot.h"
#include "MachineTasks.h"
#include "RtosCommandQueue.h"
