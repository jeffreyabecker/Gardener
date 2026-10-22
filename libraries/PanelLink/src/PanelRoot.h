#pragma once

#include "AppContext.h"

namespace gardener {

// The full device's composition root: builds the machine root, then the panel parts on top, and
// returns the two graphs. It is the only place that names a fitted panel part or includes a panel
// vendor library. The AppContext is returned by value.
AppContext buildAppContext();

}  // namespace gardener
