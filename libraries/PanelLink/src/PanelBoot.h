#pragma once

#include "PanelContext.h"
#include "storage/Storage.h"

namespace gardener {

class Logger;

// The panel's side of bring-up: the panel comes up — the display first, which is fatal when it
// does not answer — the gauge and buzzer follow, and the interpreter starts. The panel's wire
// domains, its screens and its task belong to the composition that calls this.
void panelBoot(PanelContext& context, Logger& logger, Storage::Mount mount);

}  // namespace gardener
