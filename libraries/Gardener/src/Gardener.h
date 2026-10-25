#pragma once

// Umbrella header for the Gardener firmware. A build includes this and nothing else: it names the
// machine's parts, the two links and the tasks that drive them, and — when the build has a panel —
// the display, keys, buzzer, gauge, interpreter and files arranged around the machine.

#include "Build.h"

#include "Logger.h"
#include "Machine.h"
#include "SerialLogger.h"
#include "storage/FileLibrary.h"
#include "Tasks.h"
#if GARDENER_BLUETOOTH
#include "ble/BleLink.h"
#include "ble/MachineBleDomain.h"
#endif
#include "serial/Domains.h"
#include "serial/SerialLink.h"

#if GARDENER_PANEL
#include "Panel.h"
#include "PanelRoot.h"
#include "PanelTasks.h"
#if GARDENER_BLUETOOTH
#include "ble/PanelBleDomain.h"
#endif
#include "panel/HomeUI.h"
#include "serial/PanelDomains.h"
#endif
