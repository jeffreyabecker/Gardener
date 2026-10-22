#include "storage/PanelSettings.h"

#include <string.h>

namespace gardener {
namespace {

constexpr const char* kUnitKey = "unit";
constexpr const char* kTargetKey = "target";
constexpr const char* kJingleKey = "jingle";

}  // namespace

// Reading is best effort: a value this revision does not understand leaves the default in place.
void PanelSettings::load() {
  DisplayUnit unit = DisplayUnit::Kpa;
  const String token = get(kSection, kUnitKey, displayUnitInfo(DisplayUnit::Kpa).name);
  if (parseDisplayUnit(token.c_str(), unit)) unit_ = unit;

  const float target = getFloat(kSection, kTargetKey, 0.0f);
  if (target > 0.0f) targetKpa_ = target;

  get(kSection, kJingleKey, "").toCharArray(jingle_, sizeof jingle_);
}

// The wire echoes the field after a write, so the value is applied whatever the flash does: a device
// with a filesystem that will not take the write still prints the unit it was asked for until the
// next reset, and the boot log is where the failed mount was reported.
void PanelSettings::setUnit(DisplayUnit unit) {
  if (unit == unit_) return;
  unit_ = unit;
  set(kSection, kUnitKey, displayUnitInfo(unit).name);
}

// The page clamps this to the machine's bounds before it gets here; setFloat() only keeps a
// repeated value from writing the flash.
void PanelSettings::setTarget(float kpa) {
  if (kpa == targetKpa_) return;
  targetKpa_ = kpa;
  setFloat(kSection, kTargetKey, kpa);
}

String PanelSettings::jingle() const { return String(jingle_); }

void PanelSettings::setJingle(const String& id) {
  if (id == jingle_) return;
  // Clear first so a concurrent reader always finds a terminator while the new identifier is being
  // copied; a partial value simply fails the jingles lookup and falls back to the first entry.
  memset(jingle_, 0, sizeof jingle_);
  id.toCharArray(jingle_, sizeof jingle_);
  set(kSection, kJingleKey, id);
}

}  // namespace gardener
