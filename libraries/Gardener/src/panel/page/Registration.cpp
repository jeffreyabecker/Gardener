#include "panel/page/Registration.h"

#include <any>

#include "storage/RoutineSettingStore.h"

namespace gardener {

Registration::Registration(const String& routineKey, const String& title,
                           std::vector<std::unique_ptr<SettingConfig>> settings, Factory factory,
                           String glyph)
    : routineKey_(routineKey), documentKey_(routineKey),
      title_(title.isEmpty() ? routineKey : title), owned_(std::move(settings)),
      factory_(std::move(factory)), glyph_(std::move(glyph)) {
  settings_.reserve(owned_.size());
  for (const std::unique_ptr<SettingConfig>& row : owned_) {
    if (row) settings_.push_back(row.get());
  }
}

Registration Registration::copyAs(const String& routineKey,
                                  const SectionOverrides& overrides) const {
  std::vector<std::unique_ptr<SettingConfig>> rows;
  rows.reserve(owned_.size());
  for (const std::unique_ptr<SettingConfig>& row : owned_) {
    if (!row) continue;
    rows.push_back(row->clone());
    SettingConfig* copy = rows.back().get();
    for (const SettingOverride& stated : overrides.rows) {
      if (stated.key != copy->key()) continue;
      if (stated.hidden) copy->hide();
      // Only a row that steps a number has a range for a section to move.
      if (NumericSettingCfg* numeric = dynamic_cast<NumericSettingCfg*>(copy)) {
        numeric->limit(stated.min, stated.max);
      }
    }
  }
  Registration copy(routineKey, overrides.title, std::move(rows), factory_,
                    overrides.glyph.isEmpty() ? glyph_ : overrides.glyph);
  copy.documentKey_ = documentKey_;
  copy.constants_ = overrides.constants;
  return copy;
}

SettingValues Registration::loadSettings(Library& library) const {
  if (settings_.empty()) return SettingValues();

  RoutineSettingStore store(library);
  const std::vector<const SettingKind*> kinds(settings_.begin(), settings_.end());
  SettingValues values = store.load(documentKey_, routineKey_, kinds);
  for (const SettingValues::value_type& pinned : constants_)
    values[pinned.first] = pinned.second;
  return values;
}

bool Registration::saveSettings(Library& library, SettingValues& values) const {
  if (settings_.empty()) return false;

  for (const SettingValues::value_type& pinned : constants_)
    values[pinned.first] = pinned.second;
  for (const SettingConfig* row : settings_) {
    if (values.find(row->key()) == values.end()) values[row->key()] = std::any();
  }

  RoutineSettingStore store(library);
  const std::vector<const SettingKind*> kinds(settings_.begin(), settings_.end());
  return store.save(documentKey_, routineKey_, kinds, values);
}

}  // namespace gardener