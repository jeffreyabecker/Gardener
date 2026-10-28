#include "storage/Config.h"

namespace gardener {

Config::Config(Library* store, const String& path) : store_(store), path_(path) {}

// Reading is best effort: a document that is not there or does not parse leaves the defaults in
// place. Nothing about a boot depends on the answer.
bool Config::begin() {
  bool read = false;
  if (store_ != nullptr) {
    String document;
    if (store_->read(path_, document)) {
      parse(document);
      read = true;
    }
  }

  dirty_ = false;
  return read;
}

bool Config::save() {
  if (!dirty_) return true;
  if (store_ == nullptr) return false;
  if (!store_->write(path_, text())) return false;
  dirty_ = false;
  return true;
}

// --- the document ----------------------------------------------------------

void Config::parse(const String& document) {
  sections_.clear();
  String section;
  int start = 0;
  while (start <= (int)document.length()) {
    int end = document.indexOf('\n', (unsigned int)start);
    if (end < 0) end = (int)document.length();
    String line = document.substring((unsigned int)start, (unsigned int)end);
    start = end + 1;
    line.trim();

    if (line.length() == 0) continue;
    if (line[0] == ';') continue;
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

    if (KeyValue* entry = find(section.c_str(), key.c_str())) {
      // A key that appears twice is one key with the last value, as the later line would win.
      entry->second = value;
      continue;
    }

    Section* held = findSection(section.c_str());
    if (held == nullptr) {
      sections_.push_back({section, {}});
      held = &sections_.back();
    }
    held->second.push_back({key, value});
  }
}

String Config::stored(const char* section, const char* key) const {
  if (const KeyValue* entry = find(section, key)) return entry->second;
  return "";
}

void Config::setStored(const char* section, const char* key, const String& value) {
  if (KeyValue* entry = find(section, key)) {
    entry->second = value;
    return;
  }

  Section* held = findSection(section);
  if (held == nullptr) {
    sections_.push_back({String(section), {}});
    held = &sections_.back();
  }
  held->second.push_back({String(key), value});
}

// Sections are emitted in the order they first appear, with the empty section's keys unheaded.
String Config::text() const {
  String out;
  bool first = true;
  for (const Section& section : sections_) {
    if (section.first.length() > 0) {
      if (!first) out += '\n';
      out += '[';
      out += section.first;
      out += "]\n";
    }
    for (const KeyValue& entry : section.second) {
      out += entry.first;
      out += '=';
      out += entry.second;
      out += '\n';
    }
    first = false;
  }
  return out;
}

void Config::sectionKeys(std::vector<String>& keys) const {
  keys.clear();
  for (const Section& section : sections_) {
    if (section.second.empty()) continue;
    keys.push_back(section.first);
  }
}

Config::Section* Config::findSection(const char* section) {
  for (Section& held : sections_) {
    if (held.first == section) return &held;
  }
  return nullptr;
}

const Config::Section* Config::findSection(const char* section) const {
  for (const Section& held : sections_) {
    if (held.first == section) return &held;
  }
  return nullptr;
}

Config::KeyValue* Config::find(const char* section, const char* key) {
  Section* held = findSection(section);
  if (held == nullptr) return nullptr;
  for (KeyValue& entry : held->second) {
    if (entry.first == key) return &entry;
  }
  return nullptr;
}

const Config::KeyValue* Config::find(const char* section, const char* key) const {
  const Section* held = findSection(section);
  if (held == nullptr) return nullptr;
  for (const KeyValue& entry : held->second) {
    if (entry.first == key) return &entry;
  }
  return nullptr;
}

}  // namespace gardener
