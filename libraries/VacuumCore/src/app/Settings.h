#pragma once

#include "app/Ini.h"
#include "app/Storage.h"
#include "core/Units.h"

namespace gardener {

// The settings the device keeps across a reset: the one unit the panel prints pressure in, and the
// target the default program pulls to, both in the `[device]` section of /settings.ini. Keys this
// revision does not know are carried through a write; a missing file, or a value that will not
// parse, leaves the default in place rather than refusing to boot.
//
// It is a class rather than a constant so the wire, the context and the UI read through the place a
// preference lives, and the storage is behind it rather than in every reader.
class Settings {
 public:
  explicit Settings(Storage& storage);

  void begin();
  DisplayUnit unit() const;
  void setUnit(DisplayUnit unit);
  // The depth the default program pulls to, in kPa below ambient; 0 when none has been set.
  float targetKpa() const;
  void setTarget(float kpa);

 private:
  Storage& storage_;
  Ini ini_;
  DisplayUnit unit_ = DisplayUnit::Kpa;
  float targetKpa_ = 0.0f;
};

}  // namespace gardener
