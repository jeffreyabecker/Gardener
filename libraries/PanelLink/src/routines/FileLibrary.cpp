#include "routines/FileLibrary.h"

#include <algorithm>
#include <string.h>

namespace gardener {

// Sorted, so the order a routine compiles in is the file's order rather than the directory's: a
// module that names a class another one defines has to come after it.
void FileLibrary::list(const String& folder, std::vector<String>& names) {
  names.clear();

  String path = "/";
  path += folder;
  File directory = fs_.open(path.c_str());
  if (!directory.isDirectory()) return;

  while (true) {
    File file = directory.openNextFile();
    if (!file) break;
    const String name = file.name();
    if (!file.isDirectory() && name.endsWith(".be")) names.push_back(name);
    file.close();
  }
  directory.close();

  std::sort(names.begin(), names.end(),
            [](const String& left, const String& right) {
              return strcmp(left.c_str(), right.c_str()) < 0;
            });
}

bool FileLibrary::readFile(const String& name, String& out) {
  String path = "/";
  path += name;
  File file = fs_.open(path.c_str(), "r");
  if (!file) return false;
  out = file.readString();
  file.close();
  return true;
}

}  // namespace gardener
