#pragma once

#include "core/Snapshot.h"

// One page: the device has no local controls, so the display is a readout. A page draws
// into the renderer's helpers and reads only the snapshot, so it can never reach into a
// module.

namespace eggplan {

class Renderer;

namespace pages {

void renderStatus(Renderer& renderer, const SystemSnapshot& snapshot);

}  // namespace pages
}  // namespace eggplan
