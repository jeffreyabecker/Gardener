#include "app/Storage.h"

#include <LittleFS.h>

namespace gardener {

// LittleFS is the one part of this that knows where the partition is: it mounts the `spiffs` row of
// the pinned partition table, which is the storage the build reserved. A filesystem that will not
// mount is formatted once and reported, because it is either a fresh partition or one this build
// cannot read, and the device has no other way forward.
Storage::Mount Storage::begin() {
  if (LittleFS.begin(false)) {
    ready_ = true;
    return Mount::Mounted;
  }
  if (!LittleFS.format()) return Mount::Failed;
  ready_ = LittleFS.begin(false);
  return ready_ ? Mount::Formatted : Mount::Failed;
}

bool Storage::read(const char* path, String& out) const {
  out = "";
  if (!ready_) return false;
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  out = file.readString();
  file.close();
  return true;
}

bool Storage::write(const char* path, const String& text) const {
  if (!ready_) return false;
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  size_t written = file.print(text);
  file.close();
  return written == text.length();
}

size_t Storage::size(const char* path) const {
  if (!ready_) return 0;
  File file = LittleFS.open(path, "r");
  if (!file) return 0;
  const size_t bytes = (size_t)file.size();
  file.close();
  return bytes;
}

bool Storage::read(const char* path, uint8_t* buffer, size_t capacity, size_t& length) const {
  length = 0;
  if (!ready_ || buffer == nullptr || capacity == 0) return false;
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  length = (size_t)file.read(buffer, capacity);
  file.close();
  return length > 0;
}

void Storage::list(const char* dir, const char* suffix, std::vector<String>& names) const {
  names.clear();
  if (!ready_) return;

  File folder = LittleFS.open(dir);
  if (!folder.isDirectory()) return;

  while (true) {
    File file = folder.openNextFile();
    if (!file) break;
    // A nested directory is not a file this class returns; the caller asked for names to read.
    if (!file.isDirectory()) {
      String name = file.name();
      if (!suffix || name.endsWith(suffix)) names.push_back(name);
    }
    file.close();
  }
  folder.close();
}

size_t Storage::usedBytes() const { return ready_ ? LittleFS.usedBytes() : 0; }

size_t Storage::totalBytes() const { return ready_ ? LittleFS.totalBytes() : 0; }

}  // namespace gardener
