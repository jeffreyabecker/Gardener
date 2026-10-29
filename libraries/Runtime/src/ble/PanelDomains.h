#pragma once

#include "Build.h"

#if GARDENER_BLUETOOTH

namespace gardener {

class BleDomain;
namespace panel {
class Panel;
}  // namespace panel

// The panel's slice of the GATT table: `unit` — what the panel prints — and `batt` — the gauge.
// Both have no meaning on a device without a panel.
BleDomain* createPanelBleDomain(panel::Panel& panel);

}  // namespace gardener

#endif  // GARDENER_BLUETOOTH
