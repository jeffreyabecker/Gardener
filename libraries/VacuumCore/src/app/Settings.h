#pragma once

#include "core/Units.h"

namespace gardener {

// The settings the device keeps across a reset. Today that is the one unit the panel prints pressure
// in. There is no store behind it yet, so a choice lasts until the device restarts: setUnit() moves
// the live value and nothing writes it to flash.
//
// It is a class rather than a constant so the wire, the context and the UI already read through the
// place a preference will live. Adding the store changes this file alone.
class Settings {
 public:
  DisplayUnit unit() const;
  void setUnit(DisplayUnit unit);

 private:
  DisplayUnit unit_ = DisplayUnit::Kpa;
};

}  // namespace gardener
