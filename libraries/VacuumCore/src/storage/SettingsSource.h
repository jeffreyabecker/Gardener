#pragma once

#include <Arduino.h>

namespace gardener {

// Where the device's own document lives on the filesystem it mounts. The wire's `fs` domain refuses
// to overwrite it.
constexpr const char* kDevicePath = "/settings.cfg";

// One `key=value` settings document, addressed by its full path on the filesystem the store was
// given. The callers name the file they mean: Settings reads and writes the device's own document
// at kDevicePath, and Routines builds each routine's document under /routines/. The store's only
// job is to read and write the text at the path it is handed.
class SettingsSource {
 public:
  virtual ~SettingsSource() = default;

  // The document as text. False when there is no file at that path.
  virtual bool read(const String& path, String& out) = 0;
  // Replaces the document. False when it could not be kept.
  virtual bool write(const String& path, const String& text) = 0;
};

}  // namespace gardener
