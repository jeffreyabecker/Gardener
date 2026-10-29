#include "storage/DeviceSettings.h"

namespace gardener {
namespace {

// The keys this class owns under [device].
constexpr const char* DeviceSection = "device";
constexpr const char* UnitKey = "unit";
constexpr const char* TargetKey = "target";

}  // namespace

DeviceSettings::DeviceSettings(Library* store) : document_(store) {}

bool DeviceSettings::begin() {
  const bool read = document_.begin();

  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(document_.get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  unit_ = unit;

  // A stored depth at or below zero is the ambient target, which is the same as none.
  const float target = document_.get<float>(DeviceSection, TargetKey, 0.0f);
  targetKpa_ = target > 0.0f ? target : 0.0f;

  return read;
}

void DeviceSettings::setUnit(DisplayUnit unit) {
  unit_ = unit;
  document_.set(DeviceSection, UnitKey, displayUnitInfo(unit).name);
}

void DeviceSettings::setTargetKpa(float pressureKpa) {
  targetKpa_ = pressureKpa > 0.0f ? pressureKpa : 0.0f;
  document_.set(DeviceSection, TargetKey, targetKpa_);
}

bool DeviceSettings::save() { return document_.save(); }

}  // namespace gardener
