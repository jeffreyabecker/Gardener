#pragma once

// Umbrella header for the Gardener panel library. The full device's sketch includes this one: it
// pulls in VacuumCore, which the panel depends on, and the panel's own composition pieces.

#include "VacuumCore.h"

#include "app/AppContext.h"
#include "app/PanelBoot.h"
#include "app/PanelContext.h"
#include "app/PanelRoot.h"
#include "app/PanelTasks.h"
#include "app/ble/PanelBleDomain.h"
#include "app/panel/ConnectedUI.h"
#include "app/panel/DisconnectedUI.h"
#include "app/serial/PanelDomains.h"
