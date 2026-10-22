#include "storage/Settings.h"

namespace gardener {
namespace {

constexpr const char* kBluetoothKey = "bluetooth";
constexpr const char* kLogKey = "log";

}  // namespace

Settings::Settings(FS* fs) : fs_(fs) {}

bool Settings::save() const {
  if (fs_ == nullptr) return false;
  File file = fs_->open(kPath, "w");
  if (!file) return false;
  const String body = ini_.text();
  const bool written = file.print(body) == body.length();
  file.close();
  return written;
}

// Reading is best effort: a file that is not there or does not parse leaves the defaults in place.
// Nothing about a boot depends on the answer.
bool Settings::begin() {
  bool read = false;
  if (fs_ != nullptr) {
    File file = fs_->open(kPath, "r");
    if (file) {
      ini_.parse(file.readString());
      file.close();
      read = true;
    }
  }

  bluetooth_ = getBool(kSection, kBluetoothKey, true);
  log_ = getBool(kSection, kLogKey, true);
  load();
  return read;
}

bool Settings::has(const char* section, const char* key) const { return ini_.has(section, key); }

String Settings::get(const char* section, const char* key, const char* fallback) const {
  return ini_.get(section, key, fallback);
}

bool Settings::getBool(const char* section, const char* key, bool fallback) const {
  const String stored = ini_.get(section, key, "");
  if (stored.equalsIgnoreCase("on")) return true;
  if (stored.equalsIgnoreCase("off")) return false;
  return fallback;
}

float Settings::getFloat(const char* section, const char* key, float fallback) const {
  const String stored = ini_.get(section, key, "");
  return stored.isEmpty() ? fallback : stored.toFloat();
}

bool Settings::set(const char* section, const char* key, const String& value) {
  // A repeat value is not worth a flash write.
  if (ini_.get(section, key, "") == value && ini_.has(section, key)) return true;
  if (!ini_.set(section, key, value)) return false;
  return save();
}

bool Settings::setBool(const char* section, const char* key, bool value) {
  return set(section, key, value ? "on" : "off");
}

bool Settings::setFloat(const char* section, const char* key, float value, uint8_t decimals) {
  return set(section, key, String(value, static_cast<unsigned int>(decimals)));
}

bool Settings::bluetoothEnabled() const { return bluetooth_; }

void Settings::setBluetooth(bool enabled) {
  if (enabled == bluetooth_) return;
  bluetooth_ = enabled;
  setBool(kSection, kBluetoothKey, enabled);
}

bool Settings::logEnabled() const { return log_; }

void Settings::setLogEnabled(bool enabled) {
  if (enabled == log_) return;
  log_ = enabled;
  setBool(kSection, kLogKey, enabled);
}

}  // namespace gardener
