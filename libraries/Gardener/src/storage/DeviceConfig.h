#pragma once

#include "panel/Units.h"
#include "storage/Config.h"

namespace gardener {

// The device's own preferences are the keys under `[device]` in ConfigPath: the unit the panel
// prints pressure in and the depth the default program pulls to. The jingle the device finishes
// with is not one of them; it belongs to the jingle library's own document.
constexpr const char* DeviceSection = "device";
constexpr const char* UnitKey = "unit";
constexpr const char* TargetKey = "target";

// The preferences in force while the device runs. The document is read once at boot into this, and
// a write updates it as well as the file, so a reader on the panel task never touches a String the
// interface task could rewrite.
struct DevicePreferences {
  DisplayUnit unit = DisplayUnit::Kpa;
  // The depth the default program pulls to, in kPa below ambient; 0 when none has been set.
  float targetKpa = 0.0f;
};

// Reading is best effort: a value this revision does not understand leaves the default in place.
inline void loadDevicePreferences(const Config& config, DevicePreferences& out) {
  DisplayUnit unit = DisplayUnit::Kpa;
  if (parseDisplayUnit(config.get<String>(DeviceSection, UnitKey, "").c_str(), unit)) {
    out.unit = unit;
  }

  const float target = config.get<float>(DeviceSection, TargetKey, 0.0f);
  if (target > 0.0f) out.targetKpa = target;
}

// The wire echoes the field after a write, so the value is applied whatever the flash does: a
// device with a filesystem that will not take the write still prints the unit it was asked for
// until the next reset, and the boot log is where the failed mount was reported.
inline void setDeviceUnit(Config& config, DevicePreferences& out, DisplayUnit unit) {
  if (unit == out.unit) return;
  out.unit = unit;
  config.set(DeviceSection, UnitKey, displayUnitInfo(unit).name);
}

// The page clamps this to the machine's bounds before it gets here; setDeviceTarget() only keeps a
// repeated value from writing the flash.
inline void setDeviceTarget(Config& config, DevicePreferences& out, float pressureKpa) {
  if (pressureKpa == out.targetKpa) return;
  out.targetKpa = pressureKpa;
  config.set(DeviceSection, TargetKey, pressureKpa);
}

}  // namespace gardener
