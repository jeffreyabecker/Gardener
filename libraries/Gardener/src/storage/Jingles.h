#pragma once

#include <Arduino.h>

#include <vector>

#include "storage/Config.h"

namespace gardener {

// Where the jingle library lives on the filesystem it mounts.
constexpr const char* JinglesConfigPath = "/cfg/jingles.ini";

// One jingle's entry: references into the document, so reading the library copies nothing.
struct Jingle {
  const String& key;
  const String& full_title;
  const String& short_title;
  const String& notes;
};

// The jingle library is an INI document: one section per jingle, with full_title, short_title and
// notes. Which jingle is selected is the device's own `[device] jingle` key, not the library's.
class JinglesConfig : public Config {
 public:
  // `store` is where the documents live, or null when the build has none: then nothing is read or
  // written and every value is its fallback.
  explicit JinglesConfig(Library* store);

  virtual ~JinglesConfig() = default;

  const std::vector<Jingle>& jingles() const { return jingles_; }
  // The sections that lacked one of the three keys, in document order, so the boot can name them.
  const std::vector<String>& incompleteSections() const { return incomplete_; }

 protected:
  // Restores the library; begin() calls it once the document has been read.
  void load() override;

 private:
  std::vector<Jingle> jingles_;
  std::vector<String> incomplete_;
};

}  // namespace gardener
