#pragma once

// Umbrella header for the Gardener firmware. A build includes this and nothing else: it names the
// machine's parts, the two links and the tasks that drive them, and — when the build has a panel —
// the display, keys, buzzer, gauge, interpreter and files arranged around the machine.

#include "Build.h"

#include "core/Logger.h"
#include "core/SerialLogger.h"
#include "machine/Machine.h"
#include "storage/FileLibrary.h"
#include "tasks/MachineTasks.h"
#if GARDENER_BLUETOOTH
#include "ble/BleLink.h"
#include "ble/Domains.h"
#endif
#include "serial/Domains.h"
#include "serial/SerialLink.h"

#if GARDENER_PANEL
#include "panel/HomeUI.h"
#include "panel/Panel.h"
#include "panel/PanelRoot.h"
#include "tasks/PanelTasks.h"
#if GARDENER_BLUETOOTH
#include "ble/PanelDomains.h"
#endif
#include "serial/PanelDomains.h"
#endif
