#include "storage/Storage.h"

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

bool Storage::exists(const char* path) const { return ready_ && LittleFS.exists(path); }

bool Storage::isDirectory(const char* path) const {
  if (!ready_) return false;
  File file = LittleFS.open(path);
  if (!file) return false;
  const bool directory = file.isDirectory();
  file.close();
  return directory;
}

size_t Storage::freeBytes() const {
  if (!ready_) return 0;
  const size_t total = LittleFS.totalBytes();
  const size_t used = LittleFS.usedBytes();
  return used < total ? total - used : 0;
}

class Storage::Directory {
 public:
  File file;
};

Storage::Directory* Storage::openDirectory(const char* path) const {
  if (!ready_) return nullptr;
  File folder = LittleFS.open(path);
  if (!folder || !folder.isDirectory()) return nullptr;
  Directory* directory = new Directory;
  directory->file = folder;
  return directory;
}

bool Storage::nextEntry(Directory* directory, Entry& entry) const {
  if (!directory) return false;
  File file = directory->file.openNextFile();
  if (!file) return false;
  entry.directory = file.isDirectory();
  entry.name = file.name();
  file.close();
  return true;
}

void Storage::closeDirectory(Directory* directory) const {
  if (!directory) return;
  directory->file.close();
  delete directory;
}

class Storage::ReadHandle {
 public:
  File file;
};

Storage::ReadHandle* Storage::openRead(const char* path) const {
  if (!ready_) return nullptr;
  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory()) return nullptr;
  ReadHandle* handle = new ReadHandle;
  handle->file = file;
  return handle;
}

size_t Storage::readChunk(ReadHandle* handle, uint8_t* buffer, size_t capacity) const {
  if (!handle || buffer == nullptr || capacity == 0) return 0;
  return handle->file.read(buffer, capacity);
}

void Storage::closeRead(ReadHandle* handle) const {
  if (!handle) return;
  handle->file.close();
  delete handle;
}

class Storage::WriteHandle {
 public:
  File file;
};

Storage::WriteHandle* Storage::openWrite(const char* path) const {
  if (!ready_) return nullptr;
  File file = LittleFS.open(path, "w");
  if (!file) return nullptr;
  WriteHandle* handle = new WriteHandle;
  handle->file = file;
  return handle;
}

bool Storage::writeChunk(WriteHandle* handle, const uint8_t* buffer, size_t length) const {
  if (length == 0) return true;
  if (!handle || buffer == nullptr) return false;
  return handle->file.write(buffer, length) == length;
}

void Storage::closeWrite(WriteHandle* handle) const {
  if (!handle) return;
  handle->file.flush();
  handle->file.close();
  delete handle;
}

void Storage::abortWrite(WriteHandle* handle) const {
  if (!handle) return;
  String path = handle->file.path();
  handle->file.close();
  delete handle;
  if (!path.isEmpty()) LittleFS.remove(path);
}

bool Storage::makeDirectory(const char* path) const { return ready_ && LittleFS.mkdir(path); }

bool Storage::remove(const char* path, size_t& removed) const {
  if (!ready_) return false;
  String target = path;
  File file = LittleFS.open(target);
  if (!file) return false;

  if (file.isDirectory()) {
    // Collect the children before deleting: deleting under an open cursor invalidates it.
    std::vector<String> children;
    while (true) {
      File child = file.openNextFile();
      if (!child) break;
      String childPath = target;
      if (!childPath.endsWith("/")) childPath += '/';
      childPath += child.name();
      children.push_back(childPath);
      child.close();
    }
    file.close();
    for (const String& childPath : children) {
      if (!remove(childPath.c_str(), removed)) return false;
    }
    if (!LittleFS.rmdir(target)) return false;
    removed++;
    return true;
  }

  file.close();
  if (!LittleFS.remove(target)) return false;
  removed++;
  return true;
}

size_t Storage::usedBytes() const { return ready_ ? LittleFS.usedBytes() : 0; }
size_t Storage::totalBytes() const { return ready_ ? LittleFS.totalBytes() : 0; }

}  // namespace gardener
