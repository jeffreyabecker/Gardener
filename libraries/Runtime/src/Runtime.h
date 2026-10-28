#pragma once

// Umbrella header for the Runtime library. A build includes this beside Gardener.h: the control task
// that steps the tick, the interface task that polls the links, the panel task that drives the glass
// — when the build has a panel — and the queue both links and the panel post into.

#include "Build.h"

#include "MachineTasks.h"
#include "RtosCommandQueue.h"

#if GARDENER_PANEL
#include "PanelTasks.h"
#endif
