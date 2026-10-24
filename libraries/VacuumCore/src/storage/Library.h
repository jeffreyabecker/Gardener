#pragma once

#include <Arduino.h>

#include <vector>

namespace gardener {

// The device's file store: the text documents the firmware reads and writes. The implementation
// knows the filesystem it was mounted on; callers name a full path, or a folder for the one list
// operation. Settings reads and writes the device's own document, Routines each routine's document
// and the interpreter's `.be` files, and the jingle resolver its data file — all through this one
// surface, whether the store is the device's filesystem or the host's in-memory image.
class Library {
 public:
  virtual ~Library() = default;

  // The `.be` files in one of the interpreter's folders, "system", "modules" or "routines", in load
  // order. A folder may be nested, such as "system/components".
  virtual void list(const String& folder, std::vector<String>& names) = 0;
  // A file's text, by full path. False when there is no file at that path.
  virtual bool read(const String& path, String& out) = 0;
  // Replaces the file at that path. False when it could not be kept.
  virtual bool write(const String& path, const String& text) = 0;
};

}  // namespace gardener
