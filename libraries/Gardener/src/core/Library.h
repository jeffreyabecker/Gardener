#pragma once

#include <Arduino.h>

#include <vector>

namespace gardener {

// The storage part: the device's file store, the text documents the firmware reads and writes. It is
// a boundary like the pins and the bus beside it — the implementation knows the filesystem it was
// mounted on, and nothing above it does. Callers name a full path, or a folder for the one list
// operation. Config reads and writes the device's own document and each routine's, Routines reads
// the interpreter's `.be` files — all through this one surface, whether the store is the device's
// filesystem or the host's in-memory image.
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
