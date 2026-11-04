#include "panel/page/Registration.h"

#include <any>

#include "storage/RoutineSettingStore.h"

namespace gardener {

Registration::Registration(const String& routineKey, const String& title,
                           std::vector<std::unique_ptr<SettingConfig>> settings, Factory factory,
                           String glyph)
    : routineKey_(routineKey), title_(title.isEmpty() ? routineKey : title),
      owned_(std::move(settings)), factory_(std::move(factory)), glyph_(std::move(glyph)) {
  settings_.reserve(owned_.size());
  for (const std::unique_ptr<SettingConfig>& row : owned_) {
    if (row) settings_.push_back(row.get());
  }
}

SettingValues Registration::loadSettings(Library& library) const {
  if (settings_.empty()) return SettingValues();

  RoutineSettingStore store(library);
  const std::vector<const SettingKind*> kinds(settings_.begin(), settings_.end());
  return store.load(routineKey_, kinds);
}

bool Registration::saveSettings(Library& library, SettingValues& values) const {
  if (settings_.empty()) return false;

  for (const SettingConfig* row : settings_) {
    if (values.find(row->key()) == values.end()) values[row->key()] = std::any();
  }

  RoutineSettingStore store(library);
  const std::vector<const SettingKind*> kinds(settings_.begin(), settings_.end());
  return store.save(routineKey_, kinds, values);
}

}  // namespace gardener
