#include "panel/page/RoutineRegistry.h"

#include <any>
#include <utility>

#include "core/Logger.h"
#include "storage/Config.h"
#include "storage/RoutineSettingStore.h"

namespace gardener {
namespace panel::page {

RoutineRegistry::RoutineRegistry(Library& library, Logger& logger)
    : library_(library), logger_(logger) {}

String RoutineRegistry::documentPath(const String& routineKey) const {
  String path(RoutinesConfigFolder);
  path += routineKey;
  path += ".ini";
  return path;
}

bool RoutineRegistry::add(Registration registration) {
  // The key is also the name of the document its values live in, so it has to be a plain word.
  const String routineKey = registration.routineKey();
  if (routineKey.isEmpty() || routineKey.indexOf('/') >= 0 || routineKey.indexOf('\\') >= 0) {
    logger_.warn("registry: routine key must be a plain word");
    return false;
  }

  const int16_t held = indexOf(routineKey);
  const size_t at = held < 0 ? entries_.size() : static_cast<size_t>(held);
  if (held < 0) {
    entries_.push_back({std::move(registration), {}});
  } else {
    entries_[at].registration = std::move(registration);
    entries_[at].aliases.clear();
  }

  Config document(&library_, documentPath(routineKey));
  document.begin();
  std::vector<String> sections;
  document.sectionKeys(sections);

  Entry& added = entries_[at];
  for (const String& name : sections) {
    if (name.isEmpty()) continue;
    if (entry(name) != nullptr) {
      String message = "registry: ";
      message += routineKey;
      message += " alias ";
      message += name;
      message += " already taken";
      logger_.warn(message.c_str());
      continue;
    }
    const String title = document.get<String>(name.c_str(), "routine_title", "");
    added.aliases.push_back({name, title.isEmpty() ? name : title});
  }
  return true;
}

std::vector<String> RoutineRegistry::keys() const {
  std::vector<String> out;
  for (const Entry& entry : entries_) {
    out.push_back(entry.registration.routineKey());
    for (const Alias& alias : entry.aliases) out.push_back(alias.routineKey);
  }
  return out;
}

std::vector<RoutineRow> RoutineRegistry::rows() const {
  std::vector<RoutineRow> out;
  for (const Entry& entry : entries_) {
    out.push_back({entry.registration.glyph(), entry.registration.title(),
                   entry.registration.routineKey()});
    for (const Alias& alias : entry.aliases) {
      out.push_back({entry.registration.glyph(), alias.title, alias.routineKey});
    }
  }
  return out;
}

const Registration* RoutineRegistry::entry(const String& routineKey) const {
  for (const Entry& entry : entries_) {
    for (const Alias& alias : entry.aliases) {
      if (alias.routineKey == routineKey) return &entry.registration;
    }
  }
  return registration(routineKey);
}

const Registration* RoutineRegistry::registration(const String& routineKey) const {
  const int16_t index = indexOf(routineKey);
  return index < 0 ? nullptr : &entries_[static_cast<size_t>(index)].registration;
}

int16_t RoutineRegistry::indexOf(const String& routineKey) const {
  for (size_t i = 0; i < entries_.size(); i++) {
    if (entries_[i].registration.routineKey() == routineKey) return static_cast<int16_t>(i);
  }
  return -1;
}

String RoutineRegistry::section(const String& routineKey) const {
  for (const Entry& entry : entries_) {
    for (const Alias& alias : entry.aliases) {
      if (alias.routineKey == routineKey) return routineKey;
    }
  }
  return String();
}

String RoutineRegistry::title(const String& routineKey) const {
  for (const Entry& entry : entries_) {
    for (const Alias& alias : entry.aliases) {
      if (alias.routineKey == routineKey) return alias.title;
    }
  }
  const Registration* record = registration(routineKey);
  return record == nullptr ? String() : record->title();
}

const String* RoutineRegistry::icon(const String& routineKey) const {
  const Registration* record = entry(routineKey);
  if (record == nullptr || record->glyph().isEmpty()) return nullptr;
  return &record->glyph();
}

const std::vector<SettingsRow*>* RoutineRegistry::settingsRows(
    const String& routineKey) const {
  const Registration* record = entry(routineKey);
  return record == nullptr ? nullptr : &record->settings();
}

SettingValues RoutineRegistry::loadSettings(const String& routineKey) const {
  const Registration* record = entry(routineKey);
  if (record == nullptr) return SettingValues();
  return record->loadSettings(library_, section(routineKey));
}

bool RoutineRegistry::saveSettings(const String& routineKey,
                                   SettingValues& values) const {
  const Registration* record = entry(routineKey);
  if (record == nullptr) return false;
  return record->saveSettings(library_, section(routineKey), values);
}

std::unique_ptr<Scene> RoutineRegistry::activate(
    const String& routineKey, const SettingValues& values) const {
  const Registration* record = entry(routineKey);
  if (record == nullptr || !record->factory()) return nullptr;

  RoutineActivation activation;
  activation.routineKey = routineKey;
  // The caller states what it launches with; the declaration is what holds a setting to its kind,
  // and one the set leaves out begins at its default, as one its document lacks does.
  for (SettingsRow* row : record->settings()) {
    std::any value = row->defaultValue();
    const SettingValues::const_iterator held = values.find(row->key());
    if (held != values.end()) {
      std::any given = held->second;
      if (row->accept(given)) value = std::move(given);
    }
    activation.values[row->key()] = std::move(value);
  }
  return record->factory()(activation);
}

}  // namespace panel::page
}  // namespace gardener
