#pragma once

// Umbrella header for the Gardener firmware core. A machine-alone build includes this and nothing
// else: it names the machine's parts, the two links and the tasks that drive them.

#include "app/Build.h"
#include "app/Logger.h"
#include "app/MachineBoot.h"
#include "app/NilLogger.h"
#include "app/StreamLogger.h"
#include "app/Tasks.h"
#include "app/ble/BleLink.h"
#include "app/ble/MachineBleDomain.h"
#include "app/serial/Domains.h"
#include "app/serial/SerialLink.h"
#include "core/Config.h"
