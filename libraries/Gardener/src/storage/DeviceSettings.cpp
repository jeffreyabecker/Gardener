#include "storage/DeviceSettings.h"

namespace gardener {
inline namespace storage {
namespace {

// The keys this class owns under [device].
constexpr const char* DeviceSection = "device";
constexpr const char* UnitKey = "unit";
constexpr const char* TargetKey = "target";

}  // namespace

DeviceSettings::DeviceSettings(Library* store, SystemState& state)
    : document_(store), state_(state) {}

bool DeviceSettings::begin() {
  const bool read = document_.begin();

  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(document_.get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  state_.unit = unit;

  // A stored depth at or below zero is the ambient target, which is the same as none.
  const float target = document_.get<float>(DeviceSection, TargetKey, 0.0f);
  state_.deviceTargetKpa = target > 0.0f ? target : 0.0f;

  return read;
}

void DeviceSettings::setUnit(DisplayUnit unit) {
  state_.unit = unit;
  document_.set(DeviceSection, UnitKey, displayUnitInfo(unit).name);
}

void DeviceSettings::setTargetKpa(float pressureKpa) {
  const float target = pressureKpa > 0.0f ? pressureKpa : 0.0f;
  state_.deviceTargetKpa = target;
  document_.set(DeviceSection, TargetKey, target);
}

bool DeviceSettings::save() { return document_.save(); }

}  // namespace storage
}  // namespace gardener
