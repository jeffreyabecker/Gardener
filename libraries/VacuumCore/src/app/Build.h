#pragma once

#include "app/AppContext.h"

namespace gardener {

class MachineContext;

// The machine's own composition: the pressure loop's parts, the ownership stack and the
// capability bits every build shares. A panel build composes its parts on top; a machine-alone
// build uses it alone.
MachineContext& buildMachineRoot();

// The full device's composition root: builds the machine root, then the panel parts on top, and
// returns the two graphs. It is the only place that names a fitted part or includes a vendor
// library. The AppContext is returned by value.
AppContext buildAppContext();

}  // namespace gardener
