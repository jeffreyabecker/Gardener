#include "panel/page/RoutineRegistry.h"

#include <any>
#include <cstddef>
#include <utility>

#include "core/Logger.h"

namespace gardener {

RoutineRegistry::RoutineRegistry(Library& library, Logger& logger)
    : library_(library), logger_(logger) {}

bool RoutineRegistry::add(Registration added) {
  // The key is also the name of the document its values live in, so it has to be a plain word.
  const String routineKey = added.routineKey();
  if (routineKey.isEmpty() || routineKey.indexOf('/') >= 0 || routineKey.indexOf('\\') >= 0) {
    logger_.warn("registry: routine key must be a plain word");
    return false;
  }

  const int16_t held = indexOf(routineKey);
  if (held < 0) {
    entries_.push_back(std::move(added));
  } else {
    entries_[static_cast<size_t>(held)] = std::move(added);
  }
  return true;
}

std::vector<String> RoutineRegistry::keys() const {
  std::vector<String> out;
  out.reserve(entries_.size());
  for (const Registration& entry : entries_)
    out.push_back(entry.routineKey());
  return out;
}

std::vector<RoutineRow> RoutineRegistry::rows() const {
  std::vector<RoutineRow> out;
  out.reserve(entries_.size());
  for (const Registration& entry : entries_) {
    out.push_back({entry.glyph(), entry.title(), entry.routineKey()});
  }
  return out;
}

const Registration* RoutineRegistry::registration(const String& routineKey) const {
  const int16_t index = indexOf(routineKey);
  return index < 0 ? nullptr : &entries_[static_cast<size_t>(index)];
}

int16_t RoutineRegistry::indexOf(const String& routineKey) const {
  for (size_t i = 0; i < entries_.size(); i++) {
    if (entries_[i].routineKey() == routineKey) return static_cast<int16_t>(i);
  }
  return -1;
}

String RoutineRegistry::title(const String& routineKey) const {
  const Registration* record = registration(routineKey);
  return record == nullptr ? String() : record->title();
}

const String* RoutineRegistry::icon(const String& routineKey) const {
  const Registration* record = registration(routineKey);
  if (record == nullptr || record->glyph().isEmpty()) return nullptr;
  return &record->glyph();
}

const std::vector<SettingConfig*>* RoutineRegistry::settingsRows(const String& routineKey) const {
  const Registration* record = registration(routineKey);
  return record == nullptr ? nullptr : &record->settings();
}

SettingValues RoutineRegistry::loadSettings(const String& routineKey) const {
  const Registration* record = registration(routineKey);
  if (record == nullptr) return SettingValues();
  return record->loadSettings(library_);
}

bool RoutineRegistry::saveSettings(const String& routineKey, SettingValues& values) const {
  const Registration* record = registration(routineKey);
  if (record == nullptr) return false;
  return record->saveSettings(library_, values);
}

std::unique_ptr<Scene> RoutineRegistry::activate(const String& routineKey,
                                                 const SettingValues& values) const {
  const Registration* record = registration(routineKey);
  if (record == nullptr || !record->factory()) return nullptr;

  RoutineActivation activation;
  activation.routineKey = routineKey;
  // The caller states what it launches with; the declaration is what holds a setting to its kind,
  // and one the set leaves out begins at its default, as one its document lacks does.
  for (SettingConfig* row : record->settings()) {
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

void RoutineRegistry::registerBerryRoutine(const String& routineKey) {
  if (routineKey.isEmpty() || routineKey.indexOf('/') >= 0 || routineKey.indexOf('\\') >= 0) {
    logger_.warn("registry: berry routine key must be a plain word");
    return;
  }

  if (indexOf(routineKey) < 0) {
    availableBerryRoutines_.push_back(routineKey);
  }
}

}  // namespace gardener
