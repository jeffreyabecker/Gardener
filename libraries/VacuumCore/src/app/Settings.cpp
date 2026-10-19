#include "app/Settings.h"

#include "core/Parse.h"

namespace gardener {
namespace {

constexpr const char* kPath = "/settings.ini";
constexpr const char* kSection = "device";
constexpr const char* kUnitKey = "unit";
constexpr const char* kTargetKey = "target";

}  // namespace

Settings::Settings(Storage& storage) : storage_(storage) {}

// Reading is best effort: a file that is not there, does not parse, or names a unit or target this
// revision does not understand leaves the defaults in place. Nothing about a boot depends on the
// answer.
void Settings::begin() {
  String text;
  if (!storage_.read(kPath, text)) return;

  ini_.parse(text);
  String token = ini_.get(kSection, kUnitKey, displayUnitInfo(DisplayUnit::Kpa).name);
  DisplayUnit unit = DisplayUnit::Kpa;
  if (parseDisplayUnit(token.c_str(), unit)) unit_ = unit;

  float target = 0.0f;
  String stored = ini_.get(kSection, kTargetKey, "0");
  if (parseFloat(stored.c_str(), target) && target > 0.0f) targetKpa_ = target;
}

DisplayUnit Settings::unit() const { return unit_; }

// The wire echoes the field after a write, so the value is applied whatever the flash does: a device
// with a filesystem that will not take the write still prints the unit it was asked for until the
// next reset, and the boot log is where the failed mount was reported.
void Settings::setUnit(DisplayUnit unit) {
  if (unit == unit_) return;
  unit_ = unit;
  ini_.set(kSection, kUnitKey, displayUnitInfo(unit).name);
  storage_.write(kPath, ini_.text());
}

float Settings::targetKpa() const { return targetKpa_; }

// The page clamps this to the machine's bounds before it gets here; this only keeps a repeated value
// from writing the flash.
void Settings::setTarget(float kpa) {
  if (kpa == targetKpa_) return;
  targetKpa_ = kpa;
  ini_.set(kSection, kTargetKey, String(kpa, 2u));
  storage_.write(kPath, ini_.text());
}

}  // namespace gardener
