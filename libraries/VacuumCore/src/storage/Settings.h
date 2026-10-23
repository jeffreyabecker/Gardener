#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "storage/Library.h"
#include "storage/Values.h"

namespace gardener {

// Where the device's own document lives on the filesystem it mounts. The wire's `fs` domain refuses
// to overwrite it.
constexpr const char* kDevicePath = "/settings.cfg";

// The settings the panel keeps across a reset: a flat `key=value` document, with the preferences a
// subclass names and any other key carried through a write. A missing document, or a value that
// will not parse, leaves the default in place rather than refusing to boot.
//
// The document is read and written through the store it was given, so this class does not know
// where the device keeps it beyond the path. A caller names a key; the store knows the file.
class Settings {
 public:
  // `store` is where the documents live, or null when the build has none: then nothing is read or
  // written and every value is its fallback.
  explicit Settings(Library* store);

  virtual ~Settings() = default;

  // Reads the device's document, then restores the values its subclass names. False when there was
  // nothing to read, in which case every value keeps its default.
  bool begin();

  // The general access: any key, read and written as text, as a flag or as a number. A key this
  // revision does not name is still carried through a write.
  String get(const char* key, const char* fallback) const;
  bool getBool(const char* key, bool fallback) const;
  float getFloat(const char* key, float fallback) const;

  // A write is saved at once, and a value that has not changed is not written again. False when the
  // document is full or the store would not take the write: the value is in force either way until
  // the next reset, and the boot log is where the failed mount was reported.
  bool set(const char* key, const String& value);
  bool setBool(const char* key, bool value);
  bool setFloat(const char* key, float value, uint8_t decimals = 2);

 protected:
  // Restores the values a subclass names; begin() calls it once the document has been read.
  virtual void load() {}

 private:
  bool save() const;

  Library* store_;
  Values values_;
};

}  // namespace gardener
