#pragma once

#include "core/SystemState.h"
#include "core/Units.h"
#include "storage/Config.h"

namespace gardener {

// The device's own document, /cfg/settings.ini: the unit the panel prints pressure in, the depth the
// default program pulls to, the jingle the device finishes with, and the longest run it allows. Each
// is answered as what it means rather than as the text the document holds; the unit and the depth are
// also published into the system state as they are read or written, because the overlay and the wire
// draw and report them, while the jingle and the maximum are read through this class.
//
// The document is read once, in begin(), and held from then on; every write goes through a setter,
// which sets the document's text and the held value together, and save() writes the file. A key the
// document carries that this class does not name — one a later revision adds — is carried through a
// write untouched.

// The longest run the document grants when it names none: the machine's own deadline of fifteen
// minutes, so a device that has never opened the settings list runs for the time it always has.
constexpr int32_t DefaultMaxDurationSeconds = 900;

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

  // The jingle the device finishes a run with, as the identifier /cfg/jingles.ini names it; empty
  // when none has been chosen.
  String jingle() const { return jingle_; }
  void setJingle(const String& jingle);

  // The longest run the device allows, in whole seconds.
  int32_t maxDurationSeconds() const { return maxDurationSeconds_; }
  void setMaxDurationSeconds(int32_t seconds);

  // Writes the document as it stands, once. False when the build has no store behind it.
  bool save();

 private:
  Config document_;
  // The document stays private: a caller handed it could set a key these values do not know about,
  // and then they would be stale.
  SystemState& state_;
  String jingle_;
  int32_t maxDurationSeconds_ = DefaultMaxDurationSeconds;
};

}  // namespace gardener