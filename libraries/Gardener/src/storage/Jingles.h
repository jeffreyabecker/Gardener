#pragma once

#include <Arduino.h>

#include <vector>

#include "storage/Config.h"

namespace gardener {

// Where the jingle library lives on the filesystem it mounts.
constexpr const char* JinglesConfigPath = "/system/jingles.ini";

// One jingle's entry: references into the document, so reading the library copies nothing.
struct Jingle {
  const String& key;
  const String& full_title;
  const String& short_title;
  const String& notes;
};

// The jingle library is an INI document: one section per jingle, with full_title, short_title and
// notes, and the global current key naming the selected jingle.
class JinglesConfig : public Config {
 public:
  // `store` is where the documents live, or null when the build has none: then nothing is read or
  // written and every value is its fallback.
  explicit JinglesConfig(Library* store);

  virtual ~JinglesConfig() = default;

  // The identifier the global current key names, or "" when none has been written.
  const String& currentJingleKey() const { return current_jingle_key_; }
  // Stores the selection; the value is in force whatever the flash does.
  void setCurrentJingle(const String& id);
  const std::vector<Jingle>& jingles() const { return jingles_; }
  // The sections that lacked one of the three keys, in document order, so the boot can name them.
  const std::vector<String>& incompleteSections() const { return incomplete_; }

 protected:
  // Restores the library; begin() calls it once the document has been read.
  void load() override;
  // The jingle library is its own document.
  const char* documentPath() const override { return JinglesConfigPath; }

 private:
  String current_jingle_key_;
  std::vector<Jingle> jingles_;
  std::vector<String> incomplete_;
};

}  // namespace gardener
