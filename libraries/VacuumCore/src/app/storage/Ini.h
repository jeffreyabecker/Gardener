#pragma once

#include <Arduino.h>

namespace gardener {

// A small INI document: `[section]` headers, `key=value` lines, blank lines and `#`/`;` comments.
//
// The keys it parses are kept and written back, so a file carrying keys this revision does not know
// is not emptied by a write. Comments and spacing are not kept: what comes back is the same keys in
// the same order, formatted plainly. A key before the first header is global, and stays before the
// first section because entries keep the order they were read in.
class Ini {
 public:
  // Replaces whatever was held before. Malformed lines are ignored: a settings file is not worth
  // refusing to boot over.
  void parse(const String& text);

  // The value for a key, or `fallback` when the section/key is not there.
  String get(const char* section, const char* key, const char* fallback) const;

  // Whether a key is there at all, which a get cannot say: an empty value and a missing key both
  // come back as the fallback.
  bool has(const char* section, const char* key) const { return find(section, key) != nullptr; }

  // Sets a key, adding it when it is new. False only when the table is full and the key is new.
  bool set(const char* section, const char* key, const String& value);

  // The document as text, ready to write.
  String text() const;

 private:
  struct Entry {
    String section;
    String key;
    String value;
  };

  // A settings file is a handful of keys. The limit is what makes this a plain array instead of a
  // container, and a file with more keys than this is not a settings file.
  static constexpr uint8_t kMaxEntries = 16;

  Entry* find(const char* section, const char* key);
  const Entry* find(const char* section, const char* key) const;

  Entry entries_[kMaxEntries];
  uint8_t count_ = 0;
};

}  // namespace gardener
