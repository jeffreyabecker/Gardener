#pragma once

#include "panel/Units.h"
#include "storage/Settings.h"

namespace gardener {

// The preferences the panel keeps: the one unit the panel prints pressure in, the target the default
// program pulls to, and which jingle is selected. They are the panel's because only a build with a
// panel prints a unit, pulls a target or sounds a jingle; the document, its key=value form and the
// unknown-key carry-through stay on Settings. The machine's own numbers — the pump's flow, the
// chamber's volume, the floor, the duty cap, the deadband and the valve's direction — are facts
// about the build rather than preferences, so they are build symbols in Machine.h and never reach a
// document.
class PanelSettings : public Settings {
 public:
  explicit PanelSettings(Library* store) : Settings(store) {}

  DisplayUnit unit() const { return unit_; }
  void setUnit(DisplayUnit unit);
  // The depth the default program pulls to, in kPa below ambient; 0 when none has been set.
  float targetPressureKpa() const { return targetPressureKpa_; }
  void setTarget(float pressureKpa);
  // The selected jingle's identifier, or "" when none has been chosen. The jingles module resolves
  // an empty or unknown value to its first entry.
  String jingle() const;
  void setJingle(const String& id);

 protected:
  void load() override;

 private:
  // An identifier is a short token. A fixed buffer keeps the panel's read of a preference free of
  // the String reallocation the interface task would otherwise race it with.
  static constexpr size_t kJingleIdLength = 32;

  DisplayUnit unit_ = DisplayUnit::Kpa;
  float targetPressureKpa_ = 0.0f;
  char jingle_[kJingleIdLength] = "";
};

}  // namespace gardener
