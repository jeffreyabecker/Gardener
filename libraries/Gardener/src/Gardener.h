#pragma once

// Umbrella header for the Gardener firmware. A build includes this, and the Runtime library's
// Runtime.h for the parts a board supplies and the tasks that drive them: it names the machine, the
// two links, the page stack the interpreter drives, and the interfaces the panel's parts are reached
// through. Nothing here names a pin, a bus, a file store or a vendor header.

#include "core/Logger.h"
#include "core/SerialLogger.h"
#include "machine/Machine.h"
#include "serial/Domains.h"
#include "serial/SerialLink.h"

#include "serial/PanelDomains.h"
