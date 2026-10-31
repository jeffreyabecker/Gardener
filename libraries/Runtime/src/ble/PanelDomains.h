#pragma once

namespace gardener {
inline namespace core {
class Battery;
}
inline namespace storage {
class DeviceSettings;
}
}  // namespace gardener

namespace gardener {

class BleDomain;

// The panel's slice of the GATT table: `unit` — what the panel prints — and `batt` — the gauge.
// Both have no meaning on a device without a panel.
BleDomain* createPanelBleDomain(DeviceSettings& settings, Battery& battery);

}  // namespace gardener
