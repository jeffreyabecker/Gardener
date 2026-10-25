#include "storage/Jingles.h"

namespace gardener {

JinglesConfig::JinglesConfig(Library* store) : Config(store, JinglesConfigPath) {}

// One section per jingle, with the global current key naming the selected one. A section without
// all three keys is not a jingle: it is recorded so the boot can name it and left out of the
// library. The entries point into the document, so they stay valid until the next parse or a key is
// added.
void JinglesConfig::load() {
  current_jingle_key_ = Ini::get("", "current", "");

  jingles_.clear();
  incomplete_.clear();
  for (const Section& section : sections()) {
    if (section.first.isEmpty()) continue;

    const String* fullTitle = nullptr;
    const String* shortTitle = nullptr;
    const String* notes = nullptr;
    for (const KeyValue& entry : section.second) {
      if (entry.first == "full_title") {
        fullTitle = &entry.second;
      } else if (entry.first == "short_title") {
        shortTitle = &entry.second;
      } else if (entry.first == "notes") {
        notes = &entry.second;
      }
    }
    if (fullTitle == nullptr || shortTitle == nullptr || notes == nullptr) {
      incomplete_.push_back(section.first);
      continue;
    }
    jingles_.push_back({section.first, *fullTitle, *shortTitle, *notes});
  }
}

// The stored value is applied before the write, so a device with a filesystem that will not take it
// still finishes with the jingle it was asked for until the next reset.
void JinglesConfig::setCurrentJingle(const String& id) {
  if (id == current_jingle_key_) return;
  current_jingle_key_ = id;
  set("", "current", id);
}

}  // namespace gardener
