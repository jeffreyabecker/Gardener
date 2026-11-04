#include "storage/RoutineSettingStore.h"

namespace gardener {
namespace {

String documentPath(const String& routineKey) {
  String path(RoutinesConfigFolder);
  path += routineKey;
  path += ".ini";
  return path;
}

}  // namespace

RoutineSettingStore::RoutineSettingStore(Library& library) : library_(library) {}

SettingValues RoutineSettingStore::load(const String& routineKey,
                                        const std::vector<const SettingKind*>& rows) const {
  Config document(&library_, documentPath(routineKey));
  document.begin();
  SettingValues values;
  for (const SettingKind* row : rows) {
    if (row == nullptr) continue;
    const String stored = document.get<String>("", row->key().c_str(), "");
    values[row->key()] = stored.isEmpty() ? row->defaultValue() : row->parse(stored);
  }
  return values;
}

bool RoutineSettingStore::save(const String& routineKey,
                               const std::vector<const SettingKind*>& rows,
                               const SettingValues& values) {
  Config document(&library_, documentPath(routineKey));
  document.begin();
  for (const SettingKind* row : rows) {
    if (row == nullptr) continue;
    const auto held = values.find(row->key());
    if (held == values.end()) continue;
    document.set<String>("", row->key().c_str(), row->stored(held->second));
  }
  return document.save();
}

}  // namespace gardener
