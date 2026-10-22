#include "storage/Settings.h"

#include <LittleFS.h>
#include <string.h>

#include "core/Parse.h"

namespace gardener {
namespace {

constexpr const char* kPath = "/settings.ini";
constexpr const char* kSection = "device";
constexpr const char* kUnitKey = "unit";
constexpr const char* kTargetKey = "target";
constexpr const char* kBluetoothKey = "bluetooth";
constexpr const char* kLogKey = "log";
constexpr const char* kJingleKey = "jingle";

}  // namespace

void Settings::save() const {
  File file = LittleFS.open(kPath, "w");
  if (!file) return;
  file.print(ini_.text());
  file.close();
}

// Reading is best effort: a file that is not there, does not parse, or names a unit or target this
// revision does not understand leaves the defaults in place. Nothing about a boot depends on the
// answer.
void Settings::begin() {
  File file = LittleFS.open(kPath, "r");
  if (!file) return;
  String text = file.readString();
  file.close();

  ini_.parse(text);
  String token = ini_.get(kSection, kUnitKey, displayUnitInfo(DisplayUnit::Kpa).name);
  DisplayUnit unit = DisplayUnit::Kpa;
  if (parseDisplayUnit(token.c_str(), unit)) unit_ = unit;

  float target = 0.0f;
  String stored = ini_.get(kSection, kTargetKey, "0");
  if (parseFloat(stored.c_str(), target) && target > 0.0f) targetKpa_ = target;

  bool bluetooth = true;
  stored = ini_.get(kSection, kBluetoothKey, "on");
  if (parseFlag(stored.c_str(), bluetooth)) bluetooth_ = bluetooth;

  bool log = true;
  stored = ini_.get(kSection, kLogKey, "on");
  if (parseFlag(stored.c_str(), log)) log_ = log;

  stored = ini_.get(kSection, kJingleKey, "");
  stored.toCharArray(jingle_, sizeof jingle_);
}

DisplayUnit Settings::unit() const { return unit_; }

// The wire echoes the field after a write, so the value is applied whatever the flash does: a device
// with a filesystem that will not take the write still prints the unit it was asked for until the
// next reset, and the boot log is where the failed mount was reported.
void Settings::setUnit(DisplayUnit unit) {
  if (unit == unit_) return;
  unit_ = unit;
  ini_.set(kSection, kUnitKey, displayUnitInfo(unit).name);
  save();
}

float Settings::targetKpa() const { return targetKpa_; }

// The page clamps this to the machine's bounds before it gets here; this only keeps a repeated value
// from writing the flash.
void Settings::setTarget(float kpa) {
  if (kpa == targetKpa_) return;
  targetKpa_ = kpa;
  ini_.set(kSection, kTargetKey, String(kpa, 2u));
  save();
}

bool Settings::bluetoothEnabled() const { return bluetooth_; }

void Settings::setBluetooth(bool enabled) {
  if (enabled == bluetooth_) return;
  bluetooth_ = enabled;
  ini_.set(kSection, kBluetoothKey, enabled ? "on" : "off");
  save();
}

bool Settings::logEnabled() const { return log_; }

void Settings::setLogEnabled(bool enabled) {
  if (enabled == log_) return;
  log_ = enabled;
  ini_.set(kSection, kLogKey, enabled ? "on" : "off");
  save();
}

String Settings::jingle() const { return String(jingle_); }

void Settings::setJingle(const String& id) {
  if (id == jingle_) return;
  // Clear first so a concurrent reader always finds a terminator while the new identifier is being
  // copied; a partial value simply fails the jingles lookup and falls back to the first entry.
  memset(jingle_, 0, sizeof jingle_);
  id.toCharArray(jingle_, sizeof jingle_);
  ini_.set(kSection, kJingleKey, id);
  save();
}

}  // namespace gardener
