#pragma once

#include "panel/Panel.h"

namespace gardener {

class Logger;

// The full device's composition root: the panel parts on top of the machine it builds, returned as
// the panel — the machine with the panel arranged around it. The sketch names the board it is wired
// to, and this is the only place that names a fitted panel part or includes a panel vendor library.
Panel& buildPanelRoot(Logger& logger, FS& fs, Library& library, const PanelPins& pins,
                      CommandQueue& queue);

}  // namespace gardener
