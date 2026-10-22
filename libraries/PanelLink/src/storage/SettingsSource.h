#pragma once

#include <Arduino.h>

namespace gardener {

// The name the device's own preferences are stored under. A routine's values use the routine's own
// name, which is also how they are addressed.
constexpr const char* kDeviceDocument = "device";

// Where the device's own document lives on the filesystem it mounts. The wire's `fs` domain refuses
// to overwrite it.
constexpr const char* kDevicePath = "/settings.cfg";

// The file a document is kept in. The host's image uses the same layout, so a run reads and writes
// the files the device would.
inline String settingsDocumentPath(const String& name) {
  if (name == kDeviceDocument) return String(kDevicePath);

  String path = "/routines/";
  path += name;
  path += ".cfg";
  return path;
}

// One document of `key=value` settings: the device's own, or one routine's. The name is the
// document — "device", or a routine's name — and where it lives is the implementation's business,
// so nothing that reads a value knows about a path.
class SettingsSource {
 public:
  virtual ~SettingsSource() = default;

  // The document as text. False when nothing has been stored under that name yet.
  virtual bool read(const String& name, String& out) = 0;
  // Replaces the document. False when it could not be kept.
  virtual bool write(const String& name, const String& text) = 0;
};

}  // namespace gardener
