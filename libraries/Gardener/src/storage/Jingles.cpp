#include "storage/Jingles.h"

namespace gardener {

JinglesConfig::JinglesConfig(Library* store) : Config(store) {}

// One section per jingle, with the global current key naming the selected one. A section without
// all three keys is not a jingle and is left out. The entries point into the document, so they stay
// valid until the next parse or a key is added.
void JinglesConfig::load() {
  current_jingle_key_ = Ini::get("", "current", "");

  jingles_.clear();
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
    if (fullTitle == nullptr || shortTitle == nullptr || notes == nullptr) continue;
    jingles_.push_back({section.first, *fullTitle, *shortTitle, *notes});
  }
  if (current_jingle_key_.isEmpty() && !jingles_.empty()) {
    current_jingle_key_ = jingles_.front().key;
  }
}

}  // namespace gardener
