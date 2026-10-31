#include "storage/RoutineSettingStore.h"

namespace gardener {
namespace {

// The document a routine's set lives in: the parent's, since an alias reads a section of it.
String documentPath(const String& parentRoutineKey) {
  String path(RoutinesConfigFolder);
  path += parentRoutineKey;
  path += ".ini";
  return path;
}

// The section a set lives in: the global one for the routine itself, and the section its key names for
// an alias.
String sectionOf(const String& parentRoutineKey, const String& routineKey) {
  return routineKey == parentRoutineKey ? String() : routineKey;
}

}  // namespace

RoutineSettingStore::RoutineSettingStore(Library& library) : library_(library) {}

storage::SettingValues RoutineSettingStore::load(
    const String& parentRoutineKey, const String& routineKey,
    const std::vector<const storage::SettingKind*>& rows) const {
  Config document(&library_, documentPath(parentRoutineKey));
  document.begin();
  const String section = sectionOf(parentRoutineKey, routineKey);
  storage::SettingValues values;
  for (const storage::SettingKind* row : rows) {
    if (row == nullptr) continue;
    const String stored = document.get<String>(section.c_str(), row->key().c_str(), "");
    values[row->key()] = stored.isEmpty() ? row->defaultValue() : row->parse(stored);
  }
  if (!section.isEmpty()) {
    const String title = document.get<String>(section.c_str(), "routine_title", "");
    if (!title.isEmpty()) values["routine_title"] = title;
  }
  return values;
}

bool RoutineSettingStore::save(const String& parentRoutineKey, const String& routineKey,
                               const std::vector<const storage::SettingKind*>& rows,
                               const storage::SettingValues& values) {
  Config document(&library_, documentPath(parentRoutineKey));
  document.begin();
  const String section = sectionOf(parentRoutineKey, routineKey);
  for (const storage::SettingKind* row : rows) {
    if (row == nullptr) continue;
    const auto held = values.find(row->key());
    if (held == values.end()) continue;
    document.set<String>(section.c_str(), row->key().c_str(), row->stored(held->second));
  }
  return document.save();
}

}  // namespace gardener
