#pragma once

#include "core/Units.h"

namespace gardener {

// The settings the device keeps across a reset. Today that is the one unit the panel prints pressure
// in, and there is no store yet: unit() answers the machine's own unit and setUnit() does nothing.
//
// It is a class rather than a constant so the wire, the context and the UI already read through the
// place a preference will live. Adding the store changes this file alone, and until then a write is
// accepted, dropped, and reported honestly by the echo that follows it.
class Settings {
 public:
  DisplayUnit unit() const;
  void setUnit(DisplayUnit unit);

 private:
  DisplayUnit unit_ = DisplayUnit::Kpa;
};

}  // namespace gardener
