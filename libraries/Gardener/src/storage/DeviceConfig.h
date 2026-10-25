#pragma once

namespace gardener {

// The device's own settings are the keys under `[device]` in ConfigPath: the unit the panel prints
// pressure in, the depth the default program pulls to, and the jingle the device finishes with.
// Whoever needs one reads or writes that key in the document.
constexpr const char* DeviceSection = "device";
constexpr const char* UnitKey = "unit";
constexpr const char* TargetKey = "target";
constexpr const char* JingleKey = "jingle";

}  // namespace gardener
