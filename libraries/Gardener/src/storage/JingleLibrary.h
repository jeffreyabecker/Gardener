#pragma once

#include <vector>

#include <Arduino.h>

#include "storage/Config.h"

namespace gardener {

class Logger;

// Where the jingle library lives on the filesystem it mounts.
constexpr const char* JinglesConfigPath = "/cfg/jingles.ini";

// One jingle: the identifier the device's `[device] jingle` key stores, the short label the panel
// fits, and the sequence device.play_sequence() takes.
struct Jingle {
  String key;
  String shortTitle;
  String notes;
};

// The named note sequences in /cfg/jingles.ini: one section per jingle, with short_title and notes.
// The document is read once, in begin(), and held from then on. A section missing either key is not
// a jingle: it is reported and left out, so a page offers only what the device can label and play.
class JingleLibrary {
 public:
  // `store` is where the document lives, or null when the build has none: then the library is
  // empty. `logger` names the sections left out.
  JingleLibrary(Library* store, Logger& logger);

  // Reads the document. False when there was nothing to read, in which case the library is empty.
  bool begin();

  const std::vector<Jingle>& entries() const { return entries_; }

  // The entry a key names, or nullptr when the library does not hold it.
  const Jingle* find(const String& key) const;

 private:
  Config document_;
  Logger& logger_;
  std::vector<Jingle> entries_;
};

}  // namespace gardener
