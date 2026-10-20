#include "app/storage/Ini.h"

namespace gardener {

void Ini::parse(const String& text) {
  count_ = 0;
  String section;
  int start = 0;
  while (start <= (int)text.length()) {
    int end = text.indexOf('\n', (unsigned int)start);
    if (end < 0) end = (int)text.length();
    String line = text.substring((unsigned int)start, (unsigned int)end);
    start = end + 1;
    line.trim();

    if (line.length() == 0) continue;
    if (line[0] == '#' || line[0] == ';') continue;
    if (line[0] == '[' && line.endsWith("]")) {
      section = line.substring(1, line.length() - 1);
      section.trim();
      continue;
    }

    // The first '=' separates the key from the value, so a value may contain one.
    int equals = line.indexOf('=');
    if (equals <= 0) continue;
    String key = line.substring(0, (unsigned int)equals);
    key.trim();
    if (key.length() == 0) continue;
    String value = line.substring((unsigned int)equals + 1);
    value.trim();

    if (Entry* entry = find(section.c_str(), key.c_str())) {
      // A key that appears twice is one key with the last value, as the later line would win.
      entry->value = value;
      continue;
    }
    if (count_ == kMaxEntries) continue;
    entries_[count_].section = section;
    entries_[count_].key = key;
    entries_[count_].value = value;
    count_++;
  }
}

String Ini::get(const char* section, const char* key, const char* fallback) const {
  const Entry* entry = find(section, key);
  return entry ? entry->value : String(fallback);
}

bool Ini::set(const char* section, const char* key, const String& value) {
  if (Entry* entry = find(section, key)) {
    entry->value = value;
    return true;
  }
  if (count_ == kMaxEntries) return false;
  entries_[count_].section = section;
  entries_[count_].key = key;
  entries_[count_].value = value;
  count_++;
  return true;
}

// Sections are emitted in the order they first appear, which is the order the file was read in.
// Entries are kept in that order too, so an unknown key keeps its place rather than moving.
String Ini::text() const {
  String out;
  String section;
  bool first = true;
  for (uint8_t i = 0; i < count_; i++) {
    const Entry& entry = entries_[i];
    if (entry.section != section) {
      if (!first) out += '\n';
      if (entry.section.length() > 0) {
        out += '[';
        out += entry.section;
        out += "]\n";
      }
      section = entry.section;
    }
    out += entry.key;
    out += '=';
    out += entry.value;
    out += '\n';
    first = false;
  }
  return out;
}

Ini::Entry* Ini::find(const char* section, const char* key) {
  for (uint8_t i = 0; i < count_; i++) {
    if (entries_[i].section == section && entries_[i].key == key) return &entries_[i];
  }
  return nullptr;
}

const Ini::Entry* Ini::find(const char* section, const char* key) const {
  for (uint8_t i = 0; i < count_; i++) {
    if (entries_[i].section == section && entries_[i].key == key) return &entries_[i];
  }
  return nullptr;
}

}  // namespace gardener
