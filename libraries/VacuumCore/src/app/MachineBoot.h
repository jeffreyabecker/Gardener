#pragma once

#include "app/MachineContext.h"

namespace gardener {

class Logger;

// The machine's side of bring-up: brings the machine's parts up and reports what came up. The
// links and the tasks are the composition's, so a build with no panel calls this alone.
void machineBoot(MachineContext& context, Logger& logger);

}  // namespace gardener
