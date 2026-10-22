#pragma once

// Umbrella header for the panel link: the half of the firmware that drives the panel. The full
// device's sketch includes this one — it pulls in VacuumCore, which the panel depends on, and the
// panel's own composition pieces.

#include "VacuumCore.h"

#include "AppContext.h"
#include "PanelBoot.h"
#include "PanelContext.h"
#include "PanelRoot.h"
#include "PanelTasks.h"
#include "ble/PanelBleDomain.h"
#include "panel/ConnectedUI.h"
#include "panel/DisconnectedUI.h"
#include "serial/PanelDomains.h"
