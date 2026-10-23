#pragma once

// Umbrella header for the Gardener firmware core. A machine-alone build includes this and nothing
// else: it names the machine's parts, the two links and the tasks that drive them.

#include "Build.h"

#include "Logger.h"
#include "Machine.h"
#include "SerialLogger.h"
#include "storage/FileLibrary.h"
#include "storage/MachineSettings.h"
#include "Tasks.h"
#if GARDENER_BLUETOOTH
#include "ble/BleLink.h"
#include "ble/MachineBleDomain.h"
#endif
#include "serial/Domains.h"
#include "serial/SerialLink.h"
