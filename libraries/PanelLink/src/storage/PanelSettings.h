#pragma once

#include "panel/Units.h"
#include "storage/Settings.h"

namespace gardener {

// The preferences the panel adds to the device's settings: the one unit the panel prints pressure
// in, the target the default program pulls to, and which jingle is selected. They are the panel's
// because only a build with a panel prints a unit, pulls a target or sounds a jingle; the file, its
// key=value document and the unknown-key carry-through stay on Settings.
class PanelSettings : public Settings {
 public:
  explicit PanelSettings(FS* fs) : Settings(fs) {}

  DisplayUnit unit() const { return unit_; }
  void setUnit(DisplayUnit unit);
  // The depth the default program pulls to, in kPa below ambient; 0 when none has been set.
  float targetKpa() const { return targetKpa_; }
  void setTarget(float kpa);
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
  float targetKpa_ = 0.0f;
  char jingle_[kJingleIdLength] = "";
};

}  // namespace gardener
