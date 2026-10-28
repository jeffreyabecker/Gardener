#include "Build.h"

#if GARDENER_PANEL

#include "routines/Registration.h"

#include <any>

#include "core/Logger.h"
#include "storage/RoutineSettingStore.h"

namespace gardener {

Registration::Registration(const String& routineKey, const String& title,
                           std::vector<std::unique_ptr<gfx::ui::SettingsRow>> settings, Rule rule,
                           Factory factory, gfx::ui::Glyph glyph)
    : routineKey_(routineKey),
      title_(title.isEmpty() ? routineKey : title),
      owned_(std::move(settings)),
      rule_(std::move(rule)),
      factory_(std::move(factory)),
      glyph_(std::move(glyph)) {
  settings_.reserve(owned_.size());
  for (const std::unique_ptr<gfx::ui::SettingsRow>& row : owned_) {
    if (row) settings_.push_back(row.get());
  }
}

const char* Registration::problem(const gfx::ui::SettingValues& values) const {
  return rule_ ? rule_(values) : nullptr;
}

const char* Registration::repair(const String& section, gfx::ui::SettingValues& values,
                                 Logger& logger) const {
  const char* reason = problem(values);
  if (reason == nullptr) return nullptr;

  const String label = section.isEmpty() ? routineKey_ : section;
  String message = "settings: ";
  message += label;
  message += ": ";
  message += reason;
  logger.info(message.c_str());

  // A setting already at its default is put back to the same value, so trying every row in the
  // order the list shows is the same as trying the ones that differ, and needs no comparison of
  // values held as `std::any`.
  for (const gfx::ui::SettingsRow* row : settings_) {
    values[row->key()] = row->defaultValue();
    if (problem(values) == nullptr) return reason;
  }

  message += " even at the defaults";
  logger.info(message.c_str());
  return reason;
}

gfx::ui::SettingValues Registration::loadSettings(Library& library, Logger& logger,
                                                  const String& section) const {
  gfx::ui::SettingValues values;
  if (settings_.empty()) return values;

  RoutineSettingStore store(library);
  values = store.load(routineKey_, section.isEmpty() ? routineKey_ : section, settings_);
  repair(section, values, logger);
  return values;
}

bool Registration::saveSettings(Library& library, Logger& logger, const String& section,
                                gfx::ui::SettingValues& values) const {
  if (settings_.empty()) return false;

  repair(section, values, logger);
  for (const gfx::ui::SettingsRow* row : settings_) {
    if (values.find(row->key()) == values.end()) values[row->key()] = std::any();
  }

  RoutineSettingStore store(library);
  return store.save(routineKey_, section.isEmpty() ? routineKey_ : section, settings_, values);
}

}  // namespace gardener

#endif
