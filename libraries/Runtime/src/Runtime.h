#pragma once

// Umbrella header for the Runtime library. A build includes this beside Gardener.h: the parts a
// board supplies — the pump, the sensor and the valve — the builder of those three parts, and the
// control task that steps the tick and the interface task that polls the link.

#include "MachineRoot.h"
#include "MachineTasks.h"
#include "RtosCommandQueue.h"
