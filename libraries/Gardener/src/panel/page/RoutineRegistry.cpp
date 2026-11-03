#include "panel/page/RoutineRegistry.h"

#include <any>
#include <cstddef>
#include <math.h>
#include <utility>

#include "core/Logger.h"
#include "storage/Config.h"
#include "storage/RoutineSettingStore.h"

namespace gardener {
namespace {

// What a section says about the record it copies, read beside the settings the section holds: the
// title it shows, the glyph it draws, and, for each row, what it hides, pins and moves the bounds to.
// A pinned key's text is read by the row's own kind, which is the only thing that knows what it
// means, and a pinned row is hidden as well because the list would have nothing to edit on it.
SectionOverrides overridesOf(const Config& document, const String& section,
                             const Registration& record) {
  SectionOverrides overrides;
  overrides.title = document.get<String>(section.c_str(), "routine_title", "");
  overrides.glyph = document.get<String>(section.c_str(), "routine_icon", "");
  for (const SettingConfig* row : record.settings()) {
    if (row == nullptr) continue;
    SettingOverride stated;
    stated.key = row->key();
    const String constantKey = stated.key + ":constant";
    const String text = document.get<String>(section.c_str(), constantKey.c_str(), "");
    if (!text.isEmpty()) {
      overrides.constants[stated.key] = row->parse(text);
      stated.hidden = true;
    }
    const String hiddenKey = stated.key + ":hidden";
    if (document.get<bool>(section.c_str(), hiddenKey.c_str(), false)) stated.hidden = true;
    const String minKey = stated.key + ":min";
    const String maxKey = stated.key + ":max";
    const String least = document.get<String>(section.c_str(), minKey.c_str(), "");
    const String most = document.get<String>(section.c_str(), maxKey.c_str(), "");
    if (!least.isEmpty()) stated.min = least.toFloat();
    if (!most.isEmpty()) stated.max = most.toFloat();
    if (stated.hidden || !isnan(stated.min) || !isnan(stated.max)) overrides.rows.push_back(stated);
  }
  return overrides;
}

}  // namespace

RoutineRegistry::RoutineRegistry(Library& library, Logger& logger)
    : library_(library), logger_(logger) {}

String RoutineRegistry::documentPath(const String& routineKey) const {
  String path(RoutinesConfigFolder);
  path += routineKey;
  path += ".ini";
  return path;
}

bool RoutineRegistry::add(Registration added) {
  // The key is also the name of the document its values live in, so it has to be a plain word.
  const String routineKey = added.routineKey();
  if (routineKey.isEmpty() || routineKey.indexOf('/') >= 0 || routineKey.indexOf('\\') >= 0) {
    logger_.warn("registry: routine key must be a plain word");
    return false;
  }

  const int16_t held = indexOf(routineKey);
  size_t at = entries_.size();
  if (held < 0) {
    entries_.push_back(std::move(added));
  } else {
    at = static_cast<size_t>(held);
    // The copies the old record made sit directly behind it and name it as their document: they go
    // with the record they were copied from.
    size_t last = at + 1;
    while (last < entries_.size() && entries_[last].documentKey() == routineKey)
      last++;
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(at + 1),
                   entries_.begin() + static_cast<std::ptrdiff_t>(last));
    entries_[at] = std::move(added);
  }

  Config document(&library_, documentPath(routineKey));
  document.begin();
  std::vector<String> sections;
  document.sectionKeys(sections);

  // Room for every section before a copy is taken of the record: an insert may move the entries, and
  // the record is read through a reference while its copies go in.
  entries_.reserve(entries_.size() + sections.size());
  const Registration& record = entries_[at];
  size_t after = at + 1;
  for (const String& name : sections) {
    if (name.isEmpty()) continue;
    if (registration(name) != nullptr) {
      String message = "registry: ";
      message += routineKey;
      message += " alternate ";
      message += name;
      message += " already taken";
      logger_.warn(message.c_str());
      continue;
    }
    entries_.insert(entries_.begin() + static_cast<std::ptrdiff_t>(after),
                    record.copyAs(name, overridesOf(document, name, record)));
    after++;
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
  // A setting the section pinned is the value the page is launched with, whatever the caller stated.
  for (const SettingValues::value_type& pinned : record->constants()) {
    activation.values[pinned.first] = pinned.second;
  }
  return record->factory()(activation);
}

}  // namespace gardener