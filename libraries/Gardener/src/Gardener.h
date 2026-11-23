#pragma once

// Umbrella header for the Gardener firmware. A build includes this, and the sketch's own Runtime.h
// for the builder of the board's parts and the tasks that drive them: it names the machine and the
// control a front end asks. The parts' drivers sit beside this header under core/hal, which is
// where the pins and the vendor headers are.

#include "core/Logger.h"
#include "core/SerialLogger.h"
#include "machine/Machine.h"
#include "machine/SystemControl.h"
