#include "storage/FileSettings.h"

namespace gardener {

bool FileSettings::read(const String& path, String& out) {
  File file = fs_.open(path.c_str(), "r");
  if (!file) return false;
  out = file.readString();
  file.close();
  return true;
}

bool FileSettings::write(const String& path, const String& text) {
  File file = fs_.open(path.c_str(), "w");
  if (!file) return false;
  const bool written = file.print(text) == text.length();
  file.close();
  return written;
}

}  // namespace gardener
