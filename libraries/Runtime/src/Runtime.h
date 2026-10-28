#pragma once

// Umbrella header for the Runtime library. A build includes this beside Gardener.h: the parts a
// board supplies — the pump, the sensor, the valve, the gauge, the keys, the piezo and the glass —
// the file store they read, the two links' transports and the composition that builds them, plus the
// control task that steps the tick, the interface task that polls the links, the panel task that
// drives the glass when the build has a panel, and the queue the links and the panel post into.

#include "Build.h"

#include "MachineTasks.h"
#include "MachineRoot.h"
#include "RtosCommandQueue.h"
#include "storage/FileLibrary.h"
#if GARDENER_BLUETOOTH
#include "ble/BleLink.h"
#include "ble/Domains.h"
#endif

#if GARDENER_PANEL
#include "PanelTasks.h"
#include "panel/PanelRoot.h"
#if GARDENER_BLUETOOTH
#include "ble/PanelDomains.h"
#endif
#include "serial/FsDomain.h"
#endif
