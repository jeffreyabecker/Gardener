#pragma once

// Umbrella header for the Gardener firmware. A build includes this, and the Runtime library's
// Runtime.h for the parts a board supplies and the tasks that drive them: it names the machine, the
// two links and — when the build has a panel — the display, keys, buzzer, gauge and interpreter
// arranged around the machine, over the interfaces it commands. Nothing here names a pin, a bus, a
// file store or a vendor header.

#include "Build.h"

#include "core/Logger.h"
#include "core/SerialLogger.h"
#include "machine/Machine.h"
#include "serial/Domains.h"
#include "serial/SerialLink.h"

#if GARDENER_PANEL
#include "panel/Panel.h"
#include "serial/PanelDomains.h"
#endif
