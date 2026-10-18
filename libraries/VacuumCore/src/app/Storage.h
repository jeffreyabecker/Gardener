#pragma once

#include <Arduino.h>

namespace gardener {

// The device's own files: LittleFS on the flash partition the build reserves. It is mounted once at
// boot and stays mounted, so the files a page or a setting reads are the ones on the device rather
// than a copy taken at a moment in the past.
//
// Whole file text only. Every file this firmware owns so far is a handful of lines, and the
// platform's String and File already do that without a buffer of our own.
class Storage {
 public:
  // How the mount went. A fresh partition is not a fault, but it is worth saying out loud: it
  // means the device just made an empty filesystem and had nothing to restore.
  enum class Mount { Mounted, Formatted, Failed };

  Mount begin();

  bool ready() const { return ready_; }

  // False when there is no storage or the file is not there, which the caller reads as "use the
  // default" rather than as an error to report.
  bool read(const char* path, String& out) const;
  bool write(const char* path, const String& text) const;

  size_t usedBytes() const;
  size_t totalBytes() const;

 private:
  bool ready_ = false;
};

}  // namespace gardener
