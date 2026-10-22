#pragma once

#include <Arduino.h>
#include <FS.h>
#include <stdint.h>

#include "storage/Values.h"

namespace gardener {

// The settings the device keeps across a reset: a flat `key=value` document in one file, with the
// preferences this class names and any other key carried through a write. A missing file, or a
// value that will not parse, leaves the default in place rather than refusing to boot.
//
// It is a class rather than a constant so the wire, the context and the UI read through the place a
// preference lives, and the file is read and written here rather than in every reader. The document
// behind it is the implementation: a caller names a key, not a file.
class Settings {
 public:
  // The file this class owns.
  static constexpr const char* kPath = "/settings.cfg";

  // `fs` is the filesystem the file lives on, or null when the build has none: then nothing is read
  // or written and every value is its fallback.
  explicit Settings(FS* fs);

  virtual ~Settings() = default;

  // Reads the file, then restores the values this class and its subclass name. False when there was
  // no file to read, in which case every value keeps its default.
  bool begin();

  // The general access: any key, read and written as text, as a flag or as a number. A key this
  // revision does not name is still carried through a write.
  String get(const char* key, const char* fallback) const;
  bool getBool(const char* key, bool fallback) const;
  float getFloat(const char* key, float fallback) const;

  // A write is saved at once, and a value that has not changed is not written again. False when the
  // document is full or the file would not take the write: the value is in force either way until
  // the next reset, and the boot log is where the failed mount was reported.
  bool set(const char* key, const String& value);
  bool setBool(const char* key, bool value);
  bool setFloat(const char* key, float value, uint8_t decimals = 2);

  // Whether the radio may run. BleLink::begin() reads it at boot and stays silent when it is off, so
  // a change takes effect on the next reset.
  bool bluetoothEnabled() const;
  void setBluetooth(bool enabled);

 protected:
  // Restores the values a subclass names; begin() calls it once the file has been read.
  virtual void load() {}

 private:
  bool save() const;

  FS* fs_;
  Values values_;
  bool bluetooth_ = true;
};

}  // namespace gardener
