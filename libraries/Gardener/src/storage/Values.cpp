#include "storage/Values.h"

namespace gardener {

void Values::parse(const String& text) {
  entries_.clear();
  int start = 0;
  while (start <= (int)text.length()) {
    int end = text.indexOf('\n', (unsigned int)start);
    if (end < 0) end = (int)text.length();
    String line = text.substring((unsigned int)start, (unsigned int)end);
    start = end + 1;
    line.trim();

    if (line.length() == 0) continue;
    if (line[0] == '#' || line[0] == ';') continue;

    // The first '=' separates the key from the value, so a value may contain one.
    int equals = line.indexOf('=');
    if (equals <= 0) continue;
    String key = line.substring(0, (unsigned int)equals);
    key.trim();
    if (key.length() == 0) continue;
    String value = line.substring((unsigned int)equals + 1);
    value.trim();

    if (Entry* entry = find(key.c_str())) {
      // A key that appears twice is one key with the last value, as the later line would win.
      entry->value = value;
      continue;
    }
    entries_.push_back({key, value});
  }
}

String Values::get(const char* key, const char* fallback) const {
  const Entry* entry = find(key);
  return entry ? entry->value : String(fallback);
}

void Values::set(const char* key, const String& value) {
  if (Entry* entry = find(key)) {
    entry->value = value;
    return;
  }
  entries_.push_back({key, value});
}

// Entries keep the order they were read in, so an unknown key keeps its place rather than moving.
String Values::text() const {
  String out;
  for (const Entry& entry : entries_) {
    out += entry.key;
    out += '=';
    out += entry.value;
    out += '\n';
  }
  return out;
}

Values::Entry* Values::find(const char* key) {
  for (Entry& entry : entries_) {
    if (entry.key == key) return &entry;
  }
  return nullptr;
}

const Values::Entry* Values::find(const char* key) const {
  for (const Entry& entry : entries_) {
    if (entry.key == key) return &entry;
  }
  return nullptr;
}

}  // namespace gardener
