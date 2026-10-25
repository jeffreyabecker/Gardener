#pragma once

#include <Arduino.h>
#include <stddef.h>

#include <vector>

namespace gardener {

// A flat `key=value` document: one setting per line, blank lines and `#`/`;` comments skipped.
//
// There are no sections. The keys it parses are kept and written back, so a file carrying keys this
// revision does not know is not emptied by a write. Comments and spacing are not kept: what comes
// back is the same keys in the same order, formatted plainly.
//
// The document holds every pair its text holds: nothing is refused for its position, and the table
// grows with the file rather than being sized to a limit.
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

  // How many keys the document holds, and the key and the value at a position in the order they
  // were read: enough to hand the whole document on without a `get` per key.
  size_t count() const { return entries_.size(); }
  const String& keyAt(size_t index) const { return entries_[index].key; }
  const String& valueAt(size_t index) const { return entries_[index].value; }

  // Sets a key, adding it when it is new.
  void set(const char* key, const String& value);

  // The document as text, ready to write.
  String text() const;

 private:
  struct Entry {
    String key;
    String value;
  };

  Entry* find(const char* key);
  const Entry* find(const char* key) const;

  std::vector<Entry> entries_;
};

}  // namespace gardener
