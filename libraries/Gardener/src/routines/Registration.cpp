#include "Build.h"

#if GARDENER_PANEL

#include "routines/Registration.h"

#include <any>

#include "storage/RoutineSettingStore.h"

namespace gardener {

Registration::Registration(const String& routineKey, const String& title,
                           std::vector<std::unique_ptr<panel::page::SettingsRow>> settings,
                           Factory factory, String glyph)
    : routineKey_(routineKey),
      title_(title.isEmpty() ? routineKey : title),
      owned_(std::move(settings)),
      factory_(std::move(factory)),
      glyph_(std::move(glyph)) {
  settings_.reserve(owned_.size());
  for (const std::unique_ptr<panel::page::SettingsRow>& row : owned_) {
    if (row) settings_.push_back(row.get());
  }
}

panel::page::SettingValues Registration::loadSettings(Library& library,
                                                  const String& section) const {
  panel::page::SettingValues values;
  if (settings_.empty()) return values;

  RoutineSettingStore store(library);
  values = store.load(routineKey_, section.isEmpty() ? routineKey_ : section, settings_);
  return values;
}

bool Registration::saveSettings(Library& library, const String& section,
                                panel::page::SettingValues& values) const {
  if (settings_.empty()) return false;

  for (const panel::page::SettingsRow* row : settings_) {
    if (values.find(row->key()) == values.end()) values[row->key()] = std::any();
  }

  RoutineSettingStore store(library);
  return store.save(routineKey_, section.isEmpty() ? routineKey_ : section, settings_, values);
}

}  // namespace gardener

#endif
