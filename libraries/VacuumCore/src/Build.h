#pragma once

#include "MachineContext.h"

namespace gardener {

// The machine's own composition: the pressure loop's parts, the ownership stack and the
// capability bits every build shares. A panel build composes its parts on top; a machine-alone
// build uses it alone.
MachineContext& buildMachineRoot();

}  // namespace gardener
