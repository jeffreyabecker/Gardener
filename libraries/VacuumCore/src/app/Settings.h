#pragma once

#include "app/Ini.h"
#include "app/Storage.h"
#include "core/Units.h"

namespace gardener {

// The settings the device keeps across a reset. Today that is the one unit the panel prints pressure
// in, stored as `unit=<token>` in the `[device]` section of /settings.ini. Keys this revision does
// not know are carried through a write; a missing file, or one whose unit is unreadable, leaves the
// machine's own unit in place rather than refusing to boot.
//
// It is a class rather than a constant so the wire, the context and the UI read through the place a
// preference lives, and the storage is behind it rather than in every reader.
class Settings {
 public:
  explicit Settings(Storage& storage);

  void begin();
  DisplayUnit unit() const;
  void setUnit(DisplayUnit unit);

 private:
  Storage& storage_;
  Ini ini_;
  DisplayUnit unit_ = DisplayUnit::Kpa;
};

}  // namespace gardener
