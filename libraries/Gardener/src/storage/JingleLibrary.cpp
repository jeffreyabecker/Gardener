#include "storage/JingleLibrary.h"

#include "core/Logger.h"

namespace gardener {
namespace {

constexpr const char* ShortTitleKey = "short_title";
constexpr const char* NotesKey = "notes";

}  // namespace

JingleLibrary::JingleLibrary(Library* store, Logger& logger)
    : document_(store, JinglesConfigPath), logger_(logger) {}

bool JingleLibrary::begin() {
  entries_.clear();
  if (!document_.begin()) return false;

  std::vector<String> sections;
  document_.sectionKeys(sections);
  for (const String& section : sections) {
    if (section.isEmpty()) continue;
    const String shortTitle = document_.get<String>(section.c_str(), ShortTitleKey, "");
    const String notes = document_.get<String>(section.c_str(), NotesKey, "");
    if (shortTitle.isEmpty() || notes.isEmpty()) {
      String message = "jingles: ";
      message += section;
      message += " is incomplete";
      logger_.warn(message.c_str());
      continue;
    }
    entries_.push_back({section, shortTitle, notes});
  }
  return true;
}

const Jingle* JingleLibrary::find(const String& key) const {
  for (const Jingle& jingle : entries_) {
    if (jingle.key == key) return &jingle;
  }
  return nullptr;
}

}  // namespace gardener
