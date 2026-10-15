#include "hal/Storage.h"

#include <LittleFS.h>

#include "core/Log.h"

namespace eggplan {

bool Storage::begin() {
  if (!LittleFS.begin(true)) {
    LOGE("storage: LittleFS mount failed");
    return false;
  }
  LOGI("storage: %u/%u bytes used", (unsigned)usedBytes(), (unsigned)totalBytes());
  return true;
}

bool Storage::exists(const char* path) { return LittleFS.exists(path); }

bool Storage::isDirectory(const char* path) {
  fs::File file = LittleFS.open(path);
  bool isDir = file && file.isDirectory();
  if (file) file.close();
  return isDir;
}

bool Storage::mkdir(const char* path) { return LittleFS.mkdir(path); }

bool Storage::remove(const char* path) { return LittleFS.remove(path); }

bool Storage::rename(const char* from, const char* to) { return LittleFS.rename(from, to); }

fs::File Storage::open(const char* path, const char* mode) { return LittleFS.open(path, mode); }

bool Storage::fileSize(const char* path, uint32_t& size) {
  fs::File file = LittleFS.open(path, FILE_READ);
  if (!file) return false;
  size = file.size();
  file.close();
  return true;
}

size_t Storage::listDirectory(const char* path, String* names, size_t maxNames) {
  size_t count = 0;
  fs::File directory = LittleFS.open(path);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    return 0;
  }
  fs::File entry = directory.openNextFile();
  while (entry && count < maxNames) {
    names[count++] = entry.name();
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();
  return count;
}

size_t Storage::usedBytes() { return LittleFS.usedBytes(); }

size_t Storage::totalBytes() { return LittleFS.totalBytes(); }

}  // namespace eggplan
