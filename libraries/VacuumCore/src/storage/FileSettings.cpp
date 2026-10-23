#include "storage/FileSettings.h"

namespace gardener {

// A name is a document, not a path: one that carries a separator could address another file.
// Routines refuse a routine name with one for the same reason, when it registers.
bool FileSettings::valid(const String& name) const {
  return !name.isEmpty() && name.indexOf('/') < 0 && name.indexOf('\\') < 0;
}

bool FileSettings::read(const String& name, String& out) {
  if (!valid(name)) return false;

  File file = fs_.open(settingsDocumentPath(name).c_str(), "r");
  if (!file) return false;
  out = file.readString();
  file.close();
  return true;
}

bool FileSettings::write(const String& name, const String& text) {
  if (!valid(name)) return false;

  File file = fs_.open(settingsDocumentPath(name).c_str(), "w");
  if (!file) return false;
  const bool written = file.print(text) == text.length();
  file.close();
  return written;
}

}  // namespace gardener
