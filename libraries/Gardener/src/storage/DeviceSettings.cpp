#include "storage/DeviceSettings.h"

namespace gardener {
namespace {

// The keys this class owns under [device].
constexpr const char* DeviceSection = "device";
constexpr const char* UnitKey = "unit";
constexpr const char* TargetKey = "target";
constexpr const char* JingleKey = "jingle";
constexpr const char* MaxDurationKey = "max_duration";

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

  jingle_ = document_.get<String>(DeviceSection, JingleKey, "");

  const int32_t maxDuration =
      document_.get<int>(DeviceSection, MaxDurationKey, DefaultMaxDurationSeconds);
  maxDurationSeconds_ = maxDuration > 0 ? maxDuration : 0;

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

void DeviceSettings::setJingle(const String& jingle) {
  jingle_ = jingle;
  document_.set(DeviceSection, JingleKey, jingle);
}

void DeviceSettings::setMaxDurationSeconds(int32_t seconds) {
  maxDurationSeconds_ = seconds > 0 ? seconds : 0;
  document_.set(DeviceSection, MaxDurationKey, static_cast<int>(maxDurationSeconds_));
}

bool DeviceSettings::save() { return document_.save(); }

}  // namespace gardener