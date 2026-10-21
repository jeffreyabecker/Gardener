#pragma once

#include "app/AppContext.h"

namespace gardener {

class Logger;
class PanelContext;

// The composition root: builds the whole hardware graph and returns the machine and panel graphs
// as interfaces. It is the only place outside the drivers that names a fitted part or includes a
// vendor library. The AppContext is returned by value.
AppContext buildAppContext();

// The panel's bring-up, which belongs to the composition that fits a panel,
// not to the machine's boot: it brings the panel bus up and aborts when the
// panel does not answer. A build without a panel never calls it.
bool panelBoot(PanelContext& context, Logger& logger);

}  // namespace gardener
