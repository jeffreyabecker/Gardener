#pragma once

// Umbrella header for the Gardener firmware. A build includes this, and the Runtime library's
// Runtime.h for the parts a board supplies and the tasks that drive them: it names the machine and
// the serial link in front of it. Nothing here names a pin, a bus, a file store or a vendor header.

#include "core/Logger.h"
#include "core/SerialLogger.h"
#include "machine/Machine.h"
#include "serial/SerialLink.h"
