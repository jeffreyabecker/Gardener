#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <type_traits>

#include "storage/Ini.h"
#include "storage/Library.h"

namespace gardener {

// The Config the panel keeps across a reset: an INI document, with the preferences a subclass names
// in one section and any other key carried through a write. A missing document, or a value that will
// not parse, leaves the default in place rather than refusing to boot.
//
// The document is read and written through the store it was given, so this class does not know
// where the device keeps it beyond the path. A caller names a section and a key; the store knows
// the file.
class Config {
 public:
  // `store` is where the documents live, or null when the build has none: then nothing is read or
  // written and every value is its fallback.
  explicit Config(Library* store);

  virtual ~Config() = default;

  // Reads the device's document, then restores the values its subclass names. False when there was
  // nothing to read, in which case every value keeps its default.
  bool begin();

  // The value for a section/key, read back as the fallback's type: `String`, `bool`, `float` or
  // `int`.
  template <typename T>
  T get(const char* section, const char* key, T fallback) const {
    const String stored = ini_.get(section, key, "");
    if constexpr (std::is_same<T, String>::value) {
      return stored.isEmpty() ? fallback : stored;
    } else if constexpr (std::is_same<T, bool>::value) {
      if (stored.equalsIgnoreCase("on") || stored.equalsIgnoreCase("true") ||
          stored.equalsIgnoreCase("1") || stored.equalsIgnoreCase("yes")) {
        return true;
      }
      if (stored.equalsIgnoreCase("off") || stored.equalsIgnoreCase("false") ||
          stored.equalsIgnoreCase("0") || stored.equalsIgnoreCase("no")) {
        return false;
      }
      return fallback;
    } else if constexpr (std::is_same<T, float>::value) {
      return stored.isEmpty() ? fallback : stored.toFloat();
    } 
    else if constexpr (std::is_same<T, int>::value) {
      return stored.isEmpty() ? fallback : stored.toInt();
    }
    else {
      return fallback;
    }
  }

  // The text form a section/key is written with: `on`/`off` for a bool, the number for an int or
  // float.
  template <typename T>
  bool set(const char* section, const char* key, T value, uint8_t decimals = 2) {
    String text;
    if constexpr (std::is_same<T, String>::value) {
      text = value;
    } else if constexpr (std::is_same<T, bool>::value) {
      text = value ? "on" : "off";
    } else if constexpr (std::is_same<T, float>::value) {
      text = String(value, (unsigned int)decimals);
    } else if constexpr (std::is_same<T, int>::value) {
      text = String(value);
    } else {
      return false;
    }
    if (ini_.get(section, key, "") == text && ini_.has(section, key)) return true;
    if (!ini_.set(section, key, text)) return false;
    return save();
  }

    // The names of the sections that hold at least one key, in the order they appear. The global
  // section, when it holds a key, is the empty name.
  void sectionKeys(std::vector<String>& keys) const{
    ini_.sectionKeys(keys);
  }

 protected:
  // Restores the values a subclass names; begin() calls it once the document has been read.
  virtual void load() {}

 private:
  bool save() const;

  Library* store_;
  Ini ini_;
};

}  // namespace gardener
