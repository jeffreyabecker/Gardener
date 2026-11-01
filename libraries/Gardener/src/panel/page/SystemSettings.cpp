#include "panel/page/SystemSettings.h"

#include <any>
#include <utility>

#include "core/Units.h"
#include "machine/Machine.h"
#include "panel/page/SettingsRows.h"
#include "panel/parts/BuzzerPlayer.h"
#include "storage/DeviceSettings.h"
#include "storage/JingleLibrary.h"

namespace gardener {
namespace {

// The keys the list edits. The unit and the jingle are the device's own `[device]` keys, and the
// maximum is the run length the home and static pages will hold themselves to.
constexpr const char* UnitKey = "unit";
constexpr const char* JingleKey = "jingle";
constexpr const char* MaxDurationKey = "max_duration";

constexpr const char* UnitLabel = "UNIT";
constexpr const char* JingleLabel = "JINGLE";
constexpr const char* MaxDurationLabel = "MAX";

// The mark each choice row prints before its label, as the hex digits of a packed bitmap: the
// width, the height, the format, then one byte a row. Both are eight by eight, which is the row the
// list draws them in — a gauge for the unit and a note for the jingle.
constexpr const char* UnitMark = "0808003C4285899981423C";
constexpr const char* JingleMark = "0808000E0A08080878F870";

// A minute is the step a run is thought about in, and the ceiling is the machine's own: the list
// offers exactly the longest hold the machine will grant, so it cannot promise a run the machine
// would cut short.
constexpr int32_t MaxDurationStepSeconds = 60;
constexpr int32_t MaxDurationCeilingSeconds = (int32_t)(CONTROL_MAX_RUN_MS / 1000);

}  // namespace

SystemSettings::SystemSettings(GestureQueue& gestures, Ux& ux, DeviceSettings& settings,
                               JingleLibrary& jingles, BuzzerPlayer& buzzer)
    : gestures_(gestures), ux_(ux), settings_(settings), jingles_(jingles), buzzer_(buzzer) {}

SystemSettings::~SystemSettings() = default;

void SystemSettings::build() {
  if (built_) return;
  built_ = true;

  std::vector<ChoiceItem> units;
  for (uint8_t i = 0; i < DisplayUnitCount; i++) {
    const DisplayUnitInfo& info = DisplayUnits[i];
    units.push_back({info.name, info.suffix});
  }
  owned_.push_back(std::make_unique<ChoiceRow>(gestures_, ux_, UnitKey, UnitLabel,
                                               displayUnitInfo(DisplayUnit::Kpa).name, units, false,
                                               UnitMark));

  std::vector<ChoiceItem> jingles;
  for (const Jingle& jingle : jingles_.entries()) {
    jingles.push_back({jingle.key, jingle.shortTitle});
  }
  owned_.push_back(std::make_unique<ChoiceRow>(gestures_, ux_, JingleKey, JingleLabel,
                                               jingles.empty() ? String() : jingles.front().key,
                                               jingles, false, JingleMark,
                                               [this](String& current) {
                                                 const Jingle* jingle = jingles_.find(current);
                                                 if (jingle != nullptr)
                                                   buzzer_.play(jingle->notes.c_str());
                                               }));

  owned_.push_back(std::make_unique<DurationRow>(gestures_, ux_, MaxDurationKey, MaxDurationLabel,
                                                 DefaultMaxDurationSeconds, MaxDurationStepSeconds,
                                                 0, MaxDurationCeilingSeconds, false));

  rows_.reserve(owned_.size());
  for (const std::unique_ptr<SettingsRow>& row : owned_) {
    rows_.push_back(row.get());
  }
}

const std::vector<SettingsRow*>& SystemSettings::rows() {
  build();
  return rows_;
}

SettingValues SystemSettings::values() const {
  SettingValues values;
  values[UnitKey] = String(displayUnitInfo(settings_.unit()).name);

  // A stored jingle the library no longer ships is shown as the first entry, which is what the
  // choice editor would otherwise fall back to without changing the value it holds.
  String jingle = settings_.jingle();
  if (jingles_.find(jingle) == nullptr) {
    const std::vector<Jingle>& all = jingles_.entries();
    jingle = all.empty() ? String() : all.front().key;
  }
  values[JingleKey] = jingle;

  values[MaxDurationKey] = settings_.maxDurationSeconds();
  return values;
}

bool SystemSettings::apply(const SettingValues& values) {
  const auto unit = values.find(UnitKey);
  if (unit != values.end()) {
    const String* name = std::any_cast<String>(&unit->second);
    DisplayUnit parsed = settings_.unit();
    if (name != nullptr && parseDisplayUnit(name->c_str(), parsed)) {
      settings_.setUnit(parsed);
    }
  }

  const auto jingle = values.find(JingleKey);
  if (jingle != values.end()) {
    const String* key = std::any_cast<String>(&jingle->second);
    if (key != nullptr && jingles_.find(*key) != nullptr) {
      settings_.setJingle(*key);
    }
  }

  const auto maxDuration = values.find(MaxDurationKey);
  if (maxDuration != values.end()) {
    const int32_t* seconds = std::any_cast<int32_t>(&maxDuration->second);
    if (seconds != nullptr) {
      settings_.setMaxDurationSeconds(*seconds);
    }
  }

  return settings_.save();
}

}  // namespace gardener
