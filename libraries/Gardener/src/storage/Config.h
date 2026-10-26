#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <type_traits>

#include "storage/Ini.h"
#include "storage/Library.h"

namespace gardener {

// Where the device's own document lives on the filesystem it mounts: the settings folder, with the
// jingle library and the routines' documents. The wire's `fs` domain refuses to overwrite it.
constexpr const char* ConfigPath = "/cfg/settings.ini";

// A Config is an INI document read through a store: the device's own document at ConfigPath, a
// subclass's document, or one a routine names. A key this revision does not name is carried through
// a write, and a missing document, or a value that will not parse, leaves the default in place
// rather than refusing to boot.
//
// The document is read and written through the store it was given, so this class does not know
// where the device keeps it beyond the path. A caller names a section and a key; the store knows
// the file. set() only changes the document in memory, so a caller that changes several keys pays
// one write when it calls save().
class Config : public Ini {
 public:
  // `store` is where the documents live, or null when the build has none: then nothing is read or
  // written and every value is its fallback. `path` is the document's own path, copied, so the
  // caller's name need not outlive the document.
  explicit Config(Library* store, const String& path = ConfigPath);

  virtual ~Config() = default;

  // The inherited string read stays visible beside the typed get below.
  using Ini::get;

  // Reads the document, then restores the values a subclass names. False when there was nothing to
  // read, in which case every value keeps its default.
  bool begin();

  // The value for a section/key, read back as the fallback's type: `String`, `bool`, `float` or
  // `int`.
  template <typename T>
  T get(const char* section, const char* key, T fallback) const {
    const String stored = Ini::get(section, key, "");
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
    if (Ini::get(section, key, "") == text && Ini::has(section, key)) return;
    Ini::set(section, key, text);
    dirty_ = true;
  }

  // Writes the whole document as it stands, once, and answers whether it is written. A document
  // nothing has changed since it was read is not written at all, so setting a value it already
  // holds costs no flash. False when the build has no store behind it.
  bool save();

 protected:
  // Restores the values a subclass names; begin() calls it once the document has been read.
  virtual void load() {}

 private:
  Library* store_;
  String path_;
  bool dirty_ = false;
};

}  // namespace gardener
