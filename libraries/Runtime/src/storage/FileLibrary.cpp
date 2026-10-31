#include "storage/FileLibrary.h"

#include <algorithm>
#include <string.h>

namespace gardener {

// Sorted, so the order a routine compiles in is the file's order rather than the directory's: a
// module that names a class another one defines has to come after it.
void FileLibrary::list(const String& folder, std::vector<String>& names) {
  names.clear();

  String path = "/";
  path += folder;
  std::vector<Entry> found;
  entries(path, found);
  for (const Entry& entry : found) {
    if (!entry.directory && entry.name.endsWith(".be")) names.push_back(entry.name);
  }
}

void FileLibrary::entries(const String& path, std::vector<Entry>& out) {
  out.clear();

  File directory = fs_.open(path.c_str());
  if (!directory || !directory.isDirectory()) return;

  while (true) {
    File file = directory.openNextFile();
    if (!file) break;
    out.push_back(Entry{file.name(), file.isDirectory()});
    file.close();
  }
  directory.close();

  std::sort(out.begin(), out.end(), [](const Entry& left, const Entry& right) {
    return strcmp(left.name.c_str(), right.name.c_str()) < 0;
  });
}

bool FileLibrary::read(const String& path, String& out) {
  File file = fs_.open(path.c_str(), "r");
  if (!file) return false;
  out = file.readString();
  file.close();
  return true;
}

bool FileLibrary::write(const String& path, const String& text) {
  File file = fs_.open(path.c_str(), "w");
  if (!file) return false;
  const bool written = file.print(text) == text.length();
  file.close();
  return written;
}

bool FileLibrary::exists(const String& path) { return fs_.exists(path.c_str()); }

bool FileLibrary::isDirectory(const String& path) {
  File file = fs_.open(path.c_str());
  if (!file) return false;
  const bool directory = file.isDirectory();
  file.close();
  return directory;
}

bool FileLibrary::makeDirectory(const String& path) { return fs_.mkdir(path.c_str()); }

// Deleting under an open cursor invalidates it, so a directory's children are collected before any
// of them is removed.
bool FileLibrary::remove(const String& path) {
  File file = fs_.open(path.c_str());
  if (!file) return false;

  if (!file.isDirectory()) {
    file.close();
    return fs_.remove(path.c_str());
  }

  std::vector<String> children;
  while (true) {
    File child = file.openNextFile();
    if (!child) break;
    String childPath = path;
    if (!childPath.endsWith("/")) childPath += '/';
    childPath += child.name();
    children.push_back(childPath);
    child.close();
  }
  file.close();
  for (const String& childPath : children) {
    if (!remove(childPath)) return false;
  }
  return fs_.rmdir(path.c_str());
}

}  // namespace gardener
