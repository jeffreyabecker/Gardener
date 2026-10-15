#pragma once

#include "core/Snapshot.h"

// One page: the device has no local controls, so the display is a readout. Each page
// draws into the renderer's helpers and reads only the snapshot, so a page can never
// reach into a module.

namespace eggplan {

class Renderer;

namespace pages {

void renderStatus(Renderer& renderer, const SystemSnapshot& snapshot);

}  // namespace pages
}  // namespace eggplan
