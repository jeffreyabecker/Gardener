#pragma once

// Umbrella header for the Runtime library. A build includes this beside Gardener.h: the parts a
// board supplies — the pump, the sensor, the valve, the gauge, the keys, the piezo and the glass —
// the file store they read, the serial link's transport, and the builder of the machine's three
// parts, plus the control task that steps the tick, the interface task that polls the link, the
// panel task that drives the glass, and the queue the link and the panel post into.

#include "MachineRoot.h"
#include "MachineTasks.h"
#include "RtosCommandQueue.h"
#include "RtosGestureQueue.h"
#include "RtosInternTable.h"
#include "storage/FileLibrary.h"

#include "PanelTasks.h"
