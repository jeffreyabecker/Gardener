#pragma once

// Umbrella header for the panel link: the half of the firmware that drives the panel. The full
// device's sketch includes this one — it pulls in VacuumCore, which the panel depends on, and the
// panel's own composition pieces.

#include "VacuumCore.h"

#include "Panel.h"
#include "PanelRoot.h"
#include "PanelTasks.h"
#if GARDENER_BLUETOOTH
#include "ble/PanelBleDomain.h"
#endif
#include "panel/ConnectedUI.h"
#include "panel/DisconnectedUI.h"
#include "serial/PanelDomains.h"
#include "storage/PanelSettings.h"
