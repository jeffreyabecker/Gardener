#pragma once

#include "core/SystemState.h"
#include "core/Units.h"
#include "storage/Config.h"

namespace gardener {

// The device's own document, /cfg/settings.ini: the unit the panel prints pressure in, and the depth
// the default program pulls to. Each is answered as what it means rather than as the text the
// document holds, and each is published into the system state as it is read or written, so the rest
// of the firmware asks the state and never the document.
//
// The document is read once, in begin(), and held from then on; every write goes through a setter,
// which sets the document's text and the published value together, and save() writes the file. A key
// the document carries that this class does not name — a jingle, or one a later revision adds — is
// carried through a write untouched.
class DeviceSettings {
 public:
  // `store` is where the document lives, or null when the build has none: then nothing is read or
  // written and the published values keep their defaults. `state` is the object the two values are
  // published into, owned by whoever composed the device.
  DeviceSettings(Library* store, SystemState& state);

  // Reads the document. False when there was nothing to read, in which case the published values
  // keep their defaults.
  bool begin();

  // The unit the panel prints pressure in; kPa when the document names none this firmware holds.
  DisplayUnit unit() const { return state_.unit; }
  void setUnit(DisplayUnit unit);

  // The depth the default program pulls to, in kPa below ambient; 0 when none has been set.
  float targetKpa() const { return state_.deviceTargetKpa; }
  void setTargetKpa(float pressureKpa);

  // Writes the document as it stands, once. False when the build has no store behind it.
  bool save();

 private:
  Config document_;
  // The document stays private: a caller handed it could set a key these two values do not know
  // about, and then they would be stale.
  SystemState& state_;
};

}  // namespace gardener
