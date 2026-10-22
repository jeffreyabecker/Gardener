#pragma once

#include "PanelContext.h"

namespace gardener {

// The full device's composition root: builds the machine root, then the panel parts on top, and
// returns the panel context — the machine with the panel arranged around it. It is the only place
// that names a fitted panel part or includes a panel vendor library.
PanelContext& buildPanelRoot();

}  // namespace gardener
