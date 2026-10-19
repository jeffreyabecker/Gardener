#pragma once

#include <Arduino.h>

#include <vector>

namespace gardener {

// The device's own files: LittleFS on the flash partition the build reserves. It is mounted once at
// boot and stays mounted, so the files a page or a setting reads are the ones on the device rather
// than a copy taken at a moment in the past.
//
// The text read and write are whole-file, because a settings file is a handful of lines. A transfer
// that may be large — the serial `fs` domain's read and write — streams through handles instead, so
// a file never has to fit in RAM.
class Storage {
 public:
  // How the mount went. A fresh partition is not a fault, but it is worth saying out loud: it
  // means the device just made an empty filesystem and had nothing to restore.
  enum class Mount { Mounted, Formatted, Failed };

  struct Entry {
    String name;
    bool directory = false;
  };

  // The filesystem's own cursor and transfer state, defined in Storage.cpp so LittleFS stays out of
  // this header.
  class Directory;
  class ReadHandle;
  class WriteHandle;

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

  // The file names in `dir` whose name ends with `suffix`, in the order the filesystem gives them,
  // into `names` — which is cleared first. A suffix keeps only what the caller asked for, so a
  // directory may hold other things without the filter living here. Directories are skipped; a path
  // that is not there yields nothing rather than an error, because a device with no routines
  // installed is a normal device.
  //
  // The list grows with the directory: nothing here caps how many files the device may have.
  void list(const char* dir, const char* suffix, std::vector<String>& names) const;

  bool exists(const char* path) const;
  bool isDirectory(const char* path) const;
  size_t freeBytes() const;

  // Directory traversal, one entry at a time. `openDirectory` returns null when the path is not a
  // directory this filesystem can open; each `nextEntry` yields one entry until it returns false.
  // `Entry::name` is the entry's own name, not its path.
  Directory* openDirectory(const char* path) const;
  bool nextEntry(Directory* directory, Entry& entry) const;
  void closeDirectory(Directory* directory) const;

  // Streaming transfers. `abortWrite` closes the handle and removes the partial file; `closeWrite`
  // keeps it. A short write is false, and the caller aborts.
  ReadHandle* openRead(const char* path) const;
  size_t readChunk(ReadHandle* handle, uint8_t* buffer, size_t capacity) const;
  void closeRead(ReadHandle* handle) const;

  WriteHandle* openWrite(const char* path) const;
  bool writeChunk(WriteHandle* handle, const uint8_t* buffer, size_t length) const;
  void closeWrite(WriteHandle* handle) const;
  void abortWrite(WriteHandle* handle) const;

  bool makeDirectory(const char* path) const;
  // Removes a file, or a directory with everything under it. `removed` counts what went. False
  // when any part could not be removed, which leaves the part that failed and its parents.
  bool remove(const char* path, size_t& removed) const;

  size_t usedBytes() const;
  size_t totalBytes() const;

 private:
  bool ready_ = false;
};

}  // namespace gardener
