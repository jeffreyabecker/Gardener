#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <type_traits>
#include <utility>
#include <vector>

#include "hal/Library.h"

namespace gardener {

// Where the device's own document lives on the filesystem it mounts: the settings folder, with the
// routines' documents. The wire's `fs` domain refuses to overwrite it.
constexpr const char* ConfigPath = "/cfg/settings.ini";

// An INI document read through a store: `[section]` headers, `key=value` lines, blank lines and `;`
// comments. A section's keys are held together in the order they were read, and the sections in the
// order they first appear; keys before the first header belong to the empty section, which is
// written without a header. Comments and spacing are not kept: what comes back is the same keys in
// the same order, formatted plainly.
//
// The document is read and written through the store it was given, so this class does not know
// where the device keeps it beyond the path. A key this revision does not name is carried through a
// write, and a missing document, or a value that will not parse, leaves the default in place rather
// than refusing to boot. A caller names a section and a key; the store knows the file. set() only
// changes the document in memory, so a caller that changes several keys pays one write when it
// calls save().
class Config {
 public:
  using KeyValue = std::pair<String, String>;
  using Section = std::pair<String, std::vector<KeyValue>>;

  // `store` is where the documents live, or null when the build has none: then nothing is read or
  // written and every value is its fallback. `path` is the document's own path, copied, so the
  // caller's name need not outlive the document.
  explicit Config(Library* store, const String& path = ConfigPath);

  // Reads the document. False when there was nothing to read, in which case every value keeps its
  // default.
  bool begin();

  // The value for a section/key, read back as the fallback's type: `String`, `bool`, `float` or
  // `int`.
  template <typename T>
  T get(const char* section, const char* key, T fallback) const {
    const String value = stored(section, key);
    if constexpr (std::is_same<T, String>::value) {
      return value.isEmpty() ? fallback : value;
    } else if constexpr (std::is_same<T, bool>::value) {
      if (value.equalsIgnoreCase("on") || value.equalsIgnoreCase("true") ||
          value.equalsIgnoreCase("1") || value.equalsIgnoreCase("yes")) {
        return true;
      }
      if (value.equalsIgnoreCase("off") || value.equalsIgnoreCase("false") ||
          value.equalsIgnoreCase("0") || value.equalsIgnoreCase("no")) {
        return false;
      }
      return fallback;
    } else if constexpr (std::is_same<T, float>::value) {
      return value.isEmpty() ? fallback : value.toFloat();
    } else if constexpr (std::is_same<T, int>::value) {
      return value.isEmpty() ? fallback : value.toInt();
    } else {
      return fallback;
    }
  }

  // The text form a section/key is put into the document with: `on`/`off` for a bool, the number
  // for an int or float. The document is changed in memory only; save() writes it.
  template <typename T>
  void set(const char* section, const char* key, T value, uint8_t decimals = 2) {
    String text;
    if constexpr (std::is_same<T, String>::value) {
      text = value;
    } else if constexpr (std::is_same<T, const char*>::value) {
      text = value;
    } else if constexpr (std::is_same<T, bool>::value) {
      text = value ? "on" : "off";
    } else if constexpr (std::is_same<T, float>::value) {
      text = String(value, (unsigned int)decimals);
    } else if constexpr (std::is_same<T, int>::value) {
      text = String(value);
    } else {
      return;
    }
    if (stored(section, key) == text && has(section, key)) return;
    setStored(section, key, text);
    dirty_ = true;
  }

  // Writes the whole document as it stands, once, and answers whether it is written. A document
  // nothing has changed since it was read is not written at all, so setting a value it already
  // holds costs no flash. False when the build has no store behind it.
  bool save();

  // The names of the sections that hold at least one key, in the order they appear. The global
  // section, when it holds a key, is the empty name.
  void sectionKeys(std::vector<String>& keys) const;

 private:
  // Replaces whatever was held before. Malformed lines are ignored: a settings file is not worth
  // refusing to boot over.
  void parse(const String& text);

  // The value for a section/key, or the empty string when it is not there.
  String stored(const char* section, const char* key) const;
  // Whether a section/key is there at all, which get() cannot say: an empty value and a missing key
  // both read back as the fallback.
  bool has(const char* section, const char* key) const { return find(section, key) != nullptr; }
  // Sets a key, adding its section and the key when they are new.
  void setStored(const char* section, const char* key, const String& value);

  // The document as text, ready to write.
  String text() const;

  Section* findSection(const char* section);
  const Section* findSection(const char* section) const;

  KeyValue* find(const char* section, const char* key);
  const KeyValue* find(const char* section, const char* key) const;

  Library* store_;
  String path_;
  std::vector<Section> sections_;
  bool dirty_ = false;
};

}  // namespace gardener
