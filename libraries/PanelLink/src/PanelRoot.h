#pragma once

#include "PanelContext.h"

namespace gardener {

class Logger;

// The full device's composition root: the panel parts on top of the machine it builds, returned as
// the panel context — the machine with the panel arranged around it. It is the only place that
// names a fitted panel part or includes a panel vendor library.
PanelContext& buildPanelRoot(Logger& logger, Settings& settings);

}  // namespace gardener
