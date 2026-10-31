#pragma once

#include <Arduino.h>

#include <vector>

namespace gardener {
inline namespace core {

// The storage part: the device's file store, the text documents the firmware reads and writes. It is
// a boundary like the pins and the bus beside it — the implementation knows the filesystem it was
// mounted on, and nothing above it does. Callers name a full path, or a folder for the operations
// that walk one. Config reads and writes the device's own document and each routine's, Routines reads
// the interpreter's `.be` files, the wire serves the same store to a controller — all through this
// one surface, whether the store is the device's filesystem or the host's in-memory image.
class Library {
 public:
  struct Entry {
    String name;
    bool directory;
  };

  virtual ~Library() = default;

  // The `.be` files in one of the interpreter's folders, "system", "modules" or "routines", in load
  // order. A folder may be nested, such as "system/components".
  virtual void list(const String& folder, std::vector<String>& names) = 0;
  // Everything directly under `path`, files and folders, in name order. Empty when there is nothing
  // there to walk.
  virtual void entries(const String& path, std::vector<Entry>& out) = 0;
  // A file's text, by full path. False when there is no file at that path.
  virtual bool read(const String& path, String& out) = 0;
  // Replaces the file at that path. False when it could not be kept.
  virtual bool write(const String& path, const String& text) = 0;

  virtual bool exists(const String& path) = 0;
  virtual bool isDirectory(const String& path) = 0;
  virtual bool makeDirectory(const String& path) = 0;
  // A file, or a folder with everything under it.
  virtual bool remove(const String& path) = 0;
};

}  // namespace core
}  // namespace gardener
