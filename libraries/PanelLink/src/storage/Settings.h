#pragma once

#include "storage/Ini.h"
#include "core/Units.h"

namespace gardener {

// The settings the device keeps across a reset: the one unit the panel prints pressure in, the
// target the default program pulls to, whether the radio may run, whether the diagnostics stream
// runs, and which jingle is selected, all in the `[device]` section of /settings.ini. Keys this
// revision does not know are carried through a write; a missing file, or a value that will not
// parse, leaves the default in place rather than refusing to boot.
//
// It is a class rather than a constant so the wire, the context and the UI read through the place a
// preference lives, and the file is read and written here rather than in every reader.
class Settings {
 public:
  Settings() = default;

  void begin();
  DisplayUnit unit() const;
  void setUnit(DisplayUnit unit);
  // The depth the default program pulls to, in kPa below ambient; 0 when none has been set.
  float targetKpa() const;
  void setTarget(float kpa);
  // Whether the radio may run. BleLink::begin() reads it at boot and stays silent when it is off, so
  // a change takes effect on the next reset.
  bool bluetoothEnabled() const;
  void setBluetooth(bool enabled);
  // Whether the diagnostics stream runs. The boot reads it to pick the logger (StreamLogger or
  // NilLogger), so a change takes effect on the next reset.
  bool logEnabled() const;
  void setLogEnabled(bool enabled);
  // The selected jingle's identifier, or "" when none has been chosen. The jingles module resolves
  // an empty or unknown value to its first entry.
  String jingle() const;
  void setJingle(const String& id);

 private:
  // An identifier is a short token. A fixed buffer keeps the panel's read of a preference free of
  // the String reallocation the interface task would otherwise race it with.
  static constexpr size_t kJingleIdLength = 32;

  void save() const;

  Ini ini_;
  DisplayUnit unit_ = DisplayUnit::Kpa;
  float targetKpa_ = 0.0f;
  bool bluetooth_ = true;
  bool log_ = true;
  char jingle_[kJingleIdLength] = "";
};

}  // namespace gardener
