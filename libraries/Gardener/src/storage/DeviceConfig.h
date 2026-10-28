#pragma once

namespace gardener {

// The device's own settings are the keys under `[device]` in ConfigPath: the unit the panel prints
// pressure in, and the depth the default program pulls to. Whoever needs one reads or writes that
// key in the document.
constexpr const char* DeviceSection = "device";
constexpr const char* UnitKey = "unit";
constexpr const char* TargetKey = "target";

}  // namespace gardener
