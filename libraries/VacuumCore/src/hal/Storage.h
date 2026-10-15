#pragma once

#include <Arduino.h>
#include <FS.h>

namespace eggplan {

// Accepted exception: this header exposes fs::File. A port would hide the ESP
// filesystem types, but the project deliberately has no interfaces, and
// services/ProgramStore is the only consumer.
class Storage {
 public:
  bool begin();

  bool exists(const char* path);
  bool isDirectory(const char* path);
  bool mkdir(const char* path);
  bool remove(const char* path);
  bool rename(const char* from, const char* to);
  fs::File open(const char* path, const char* mode);
  bool fileSize(const char* path, uint32_t& size);

  // Fills names with up to maxNames entries; returns the count found.
  size_t listDirectory(const char* path, String* names, size_t maxNames);

  size_t usedBytes();
  size_t totalBytes();
};

}  // namespace eggplan
