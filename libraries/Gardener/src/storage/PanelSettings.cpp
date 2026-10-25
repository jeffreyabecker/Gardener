#include "Build.h"

#if GARDENER_PANEL

#include "storage/PanelSettings.h"

#include <string.h>

namespace gardener {
namespace {

constexpr const char* UnitKey = "unit";
constexpr const char* TargetKey = "target";
constexpr const char* JingleKey = "jingle";

}  // namespace

// Reading is best effort: a value this revision does not understand leaves the default in place.
void PanelSettings::load() {
  DisplayUnit unit = DisplayUnit::Kpa;
  const String token = get(UnitKey, displayUnitInfo(DisplayUnit::Kpa).name);
  if (parseDisplayUnit(token.c_str(), unit)) unit_ = unit;

  const float target = getFloat(TargetKey, 0.0f);
  if (target > 0.0f) targetPressureKpa_ = target;

  get(JingleKey, "").toCharArray(jingle_, sizeof jingle_);
}

// The wire echoes the field after a write, so the value is applied whatever the flash does: a device
// with a filesystem that will not take the write still prints the unit it was asked for until the
// next reset, and the boot log is where the failed mount was reported.
void PanelSettings::setUnit(DisplayUnit unit) {
  if (unit == unit_) return;
  unit_ = unit;
  set(UnitKey, displayUnitInfo(unit).name);
}

// The page clamps this to the machine's bounds before it gets here; setFloat() only keeps a
// repeated value from writing the flash.
void PanelSettings::setTarget(float pressureKpa) {
  if (pressureKpa == targetPressureKpa_) return;
  targetPressureKpa_ = pressureKpa;
  setFloat(TargetKey, pressureKpa);
}

String PanelSettings::jingle() const { return String(jingle_); }

void PanelSettings::setJingle(const String& id) {
  if (id == jingle_) return;
  // Clear first so a concurrent reader always finds a terminator while the new identifier is being
  // copied; a partial value simply fails the jingles lookup and falls back to the first entry.
  memset(jingle_, 0, sizeof jingle_);
  id.toCharArray(jingle_, sizeof jingle_);
  set(JingleKey, id);
}

}  // namespace gardener
#endif
