#pragma once

// Umbrella header for the Runtime library. A build includes this beside Gardener.h: the parts a
// board supplies — the pump, the sensor, the valve, the gauge, the keys, the piezo and the glass —
// the file store they read, the two links' transports, and the builder of the machine's three parts,
// plus the control task that steps the tick, the interface task that polls the links, the panel task
// that drives the glass, and the queue the links and the panel post into.

#include "MachineTasks.h"
#include "MachineRoot.h"
#include "RtosCommandQueue.h"
#include "storage/FileLibrary.h"
#include "ble/BleLink.h"
#include "ble/Domains.h"

#include "PanelTasks.h"
#include "ble/PanelDomains.h"
#include "serial/FsDomain.h"
