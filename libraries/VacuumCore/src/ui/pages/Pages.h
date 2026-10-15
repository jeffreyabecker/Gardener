#pragma once

#include <stddef.h>

#include "core/Snapshot.h"

// One file per page. Each draws into the renderer's helpers and reads only the
// snapshot, so a page can never reach into a module.

namespace eggplan {

class Renderer;

namespace pages {

void renderStatus(Renderer& renderer, const SystemSnapshot& snapshot);
void renderPrograms(Renderer& renderer, const SystemSnapshot& snapshot, size_t cursor);
void renderSetpoint(Renderer& renderer, const SystemSnapshot& snapshot);
void renderGraph(Renderer& renderer, const SystemSnapshot& snapshot);
void renderInfo(Renderer& renderer, const SystemSnapshot& snapshot);

}  // namespace pages
}  // namespace eggplan
