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

  // How many names one listing can return. A directory of the device's own files is small, and a
  // fixed bound is what keeps the caller's buffer on its own stack.
  static constexpr uint8_t kMaxNames = 16;

  Mount begin();

  bool ready() const { return ready_; }

  // False when there is no storage or the file is not there, which the caller reads as "use the
  // default" rather than as an error to report.
  bool read(const char* path, String& out) const;
  bool write(const char* path, const String& text) const;

  // An image's bytes are not text — a palette is full of zero bytes, which a String read stops at —
  // so binary files have their own size and read. `length` is what the file gave, which is the whole
  // file unless `capacity` is smaller.
  size_t size(const char* path) const;
  bool read(const char* path, uint8_t* buffer, size_t capacity, size_t& length) const;

  // The file names in a directory, at most `max` of them, into `names`. A suffix keeps only what
  // the caller asked for, so a directory may hold other things without the filter living here.
  // Directories are skipped; a path that is not there yields zero names rather than an error,
  // because a device with no routines installed is a normal device.
  uint8_t list(const char* dir, const char* suffix, String* names, uint8_t max) const;

  size_t usedBytes() const;
  size_t totalBytes() const;

 private:
  bool ready_ = false;
};

}  // namespace gardener
