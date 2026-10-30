#pragma once

#include "panel/Panel.h"

namespace gardener {

class Logger;
struct SystemState;

// The full device's composition root: the machine parts and the panel parts, returned as the panel —
// the machine with the panel arranged around it. The sketch names the board it is wired to, and this
// is the only place that names a fitted part or includes a part's vendor library. `state` is the
// object the panel's document publishes its two values into.
panel::Panel& buildPanelRoot(Logger& logger, Library& library, const panel::PanelPins& pins,
                             CommandQueue& queue, SystemState& state);

// The board's half of the panel's bring-up: the display bus comes up on its two pins, then the panel
// brings its parts up and answers whether the glass did. The panel is the one part with no fallback,
// so a panel that does not answer is a panic and a reset rather than a device that comes up blind.
void bootPanel(panel::Panel& panel, const panel::PanelPins& pins);

}  // namespace gardener
