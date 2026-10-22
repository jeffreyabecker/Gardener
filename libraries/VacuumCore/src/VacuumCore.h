#pragma once

// Umbrella header for the Gardener firmware core. A machine-alone build includes this and nothing
// else: it names the machine's parts, the two links and the tasks that drive them.

#include "Logger.h"
#include "Machine.h"
#include "NilLogger.h"
#include "StreamLogger.h"
#include "Tasks.h"
#include "ble/BleLink.h"
#include "ble/MachineBleDomain.h"
#include "serial/Domains.h"
#include "serial/SerialLink.h"
#include "storage/Settings.h"
