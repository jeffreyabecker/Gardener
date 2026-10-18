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

size_t Storage::usedBytes() const { return ready_ ? LittleFS.usedBytes() : 0; }

size_t Storage::totalBytes() const { return ready_ ? LittleFS.totalBytes() : 0; }

}  // namespace gardener
