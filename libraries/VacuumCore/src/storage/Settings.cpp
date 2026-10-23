#include "storage/Settings.h"

namespace gardener {

Settings::Settings(SettingsSource* source) : source_(source) {}

bool Settings::save() const {
  if (source_ == nullptr) return false;
  return source_->write(kDevicePath, values_.text());
}

// Reading is best effort: a document that is not there or does not parse leaves the defaults in
// place. Nothing about a boot depends on the answer.
bool Settings::begin() {
  bool read = false;
  if (source_ != nullptr) {
    String text;
    if (source_->read(kDevicePath, text)) {
      values_.parse(text);
      read = true;
    }
  }

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

}  // namespace gardener
