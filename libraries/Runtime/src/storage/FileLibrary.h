#pragma once

#include <FS.h>

#include "storage/Library.h"

namespace gardener {

// A Library on the filesystem the composition root mounted: it opens the path it is handed and
// carries no layout of its own. The callers build the paths — Config names the device's document and
// the jingle library, Routines builds each routine's, and the interpreter's folders are listed by
// name — so what is safe to open is their business, not the store's.
class FileLibrary : public Library {
 public:
  explicit FileLibrary(FS& fs) : fs_(fs) {}

  void list(const String& folder, std::vector<String>& names) override;
  bool read(const String& path, String& out) override;
  bool write(const String& path, const String& text) override;

 private:
  FS& fs_;
};

}  // namespace gardener
