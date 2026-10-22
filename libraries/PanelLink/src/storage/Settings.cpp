#include "storage/Settings.h"

namespace gardener {
namespace {

constexpr const char* kBluetoothKey = "bluetooth";

}  // namespace

Settings::Settings(FS* fs) : fs_(fs) {}

bool Settings::save() const {
  if (fs_ == nullptr) return false;
  File file = fs_->open(kPath, "w");
  if (!file) return false;
  const String body = values_.text();
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
      values_.parse(file.readString());
      file.close();
      read = true;
    }
  }

  bluetooth_ = getBool(kBluetoothKey, true);
  load();
  return read;
}

String Settings::get(const char* key, const char* fallback) const {
  return values_.get(key, fallback);
}

bool Settings::getBool(const char* key, bool fallback) const {
  const String stored = values_.get(key, "");
  if (stored.equalsIgnoreCase("on")) return true;
  if (stored.equalsIgnoreCase("off")) return false;
  return fallback;
}

float Settings::getFloat(const char* key, float fallback) const {
  const String stored = values_.get(key, "");
  return stored.isEmpty() ? fallback : stored.toFloat();
}

bool Settings::set(const char* key, const String& value) {
  // A repeat value is not worth a flash write.
  if (values_.get(key, "") == value && values_.has(key)) return true;
  if (!values_.set(key, value)) return false;
  return save();
}

bool Settings::setBool(const char* key, bool value) {
  return set(key, value ? "on" : "off");
}

bool Settings::setFloat(const char* key, float value, uint8_t decimals) {
  return set(key, String(value, static_cast<unsigned int>(decimals)));
}

bool Settings::bluetoothEnabled() const { return bluetooth_; }

void Settings::setBluetooth(bool enabled) {
  if (enabled == bluetooth_) return;
  bluetooth_ = enabled;
  setBool(kBluetoothKey, enabled);
}

}  // namespace gardener
