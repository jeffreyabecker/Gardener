#pragma once

#include <memory>
#include <vector>

#include <Arduino.h>

#include "storage/SettingKind.h"

namespace gardener {

class BuzzerPlayer;
class DeviceSettings;
class GestureQueue;
class JingleLibrary;
class SettingsRow;
class Ux;

// The device's own settings as a list: the rows the system page shows — the unit the glass prints,
// the jingle the device finishes with, and the longest run it allows — held for the life of the
// device because the list borrows them, and the values they read and write through the document.
//
// The rows are built on the first call rather than in the constructor: the jingle choices come from
// the library the composition reads at boot, which is after this object exists.
class SystemSettings {
 public:
  SystemSettings(GestureQueue& gestures, Ux& ux, DeviceSettings& settings, JingleLibrary& jingles,
                 BuzzerPlayer& buzzer);
  ~SystemSettings();

  SystemSettings(const SystemSettings&) = delete;
  SystemSettings& operator=(const SystemSettings&) = delete;

  // The rows the list shows, in order, built once on the first call.
  const std::vector<SettingsRow*>& rows();

  // The set the rows hold now, read from the device: the unit's wire token, the jingle the library
  // holds or its first entry, and the maximum in whole seconds.
  SettingValues values() const;

  // A settled set written back through the device: the unit and the jingle are taken only as the
  // device and its library name them, and the maximum as whole seconds. False when the document
  // could not be written.
  bool apply(const SettingValues& values);

 private:
  void build();

  GestureQueue& gestures_;
  Ux& ux_;
  DeviceSettings& settings_;
  JingleLibrary& jingles_;
  BuzzerPlayer& buzzer_;
  std::vector<std::unique_ptr<SettingsRow>> owned_;
  std::vector<SettingsRow*> rows_;
  bool built_ = false;
};

}  // namespace gardener
