#include "app/Settings.h"

namespace gardener {
namespace {

constexpr const char* kPath = "/settings.ini";
constexpr const char* kSection = "device";
constexpr const char* kUnitKey = "unit";

}  // namespace

Settings::Settings(Storage& storage) : storage_(storage) {}

// Reading is best effort: a file that is not there, does not parse, or names a unit this revision
// does not know leaves the defaults in place. Nothing about a boot depends on the answer.
void Settings::begin() {
  String text;
  if (!storage_.read(kPath, text)) return;

  ini_.parse(text);
  String token = ini_.get(kSection, kUnitKey, displayUnitInfo(DisplayUnit::Kpa).name);
  DisplayUnit unit = DisplayUnit::Kpa;
  if (parseDisplayUnit(token.c_str(), unit)) unit_ = unit;
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

}  // namespace gardener
