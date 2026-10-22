#pragma once

#include <Arduino.h>

namespace gardener {

// A flat `key=value` document: one setting per line, blank lines and `#`/`;` comments skipped.
//
// There are no sections. The keys it parses are kept and written back, so a file carrying keys this
// revision does not know is not emptied by a write. Comments and spacing are not kept: what comes
// back is the same keys in the same order, formatted plainly.
class Values {
 public:
  // Replaces whatever was held before. Malformed lines are ignored: a settings file is not worth
  // refusing to boot over.
  void parse(const String& text);

  // The value for a key, or `fallback` when the key is not there.
  String get(const char* key, const char* fallback) const;

  // Whether a key is there at all, which a get cannot say: an empty value and a missing key both
  // come back as the fallback.
  bool has(const char* key) const { return find(key) != nullptr; }

  // Sets a key, adding it when it is new. False only when the table is full and the key is new.
  bool set(const char* key, const String& value);

  // The document as text, ready to write.
  String text() const;

 private:
  struct Entry {
    String key;
    String value;
  };

  // A values file is a handful of keys. The limit is what makes this a plain array instead of a
  // container, and a file with more keys than this is not a values file.
  static constexpr uint8_t kMaxEntries = 32;

  Entry* find(const char* key);
  const Entry* find(const char* key) const;

  Entry entries_[kMaxEntries];
  uint8_t count_ = 0;
};

}  // namespace gardener
