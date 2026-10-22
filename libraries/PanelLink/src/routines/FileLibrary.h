#pragma once

#include <FS.h>

#include "routines/Library.h"

namespace gardener {

// The interpreter's files on the filesystem the composition root mounted: a folder per kind, and a
// data file such as the jingle library read by name.
class FileLibrary : public Library {
 public:
  explicit FileLibrary(FS& fs) : fs_(fs) {}

  void list(const String& folder, std::vector<String>& names) override;
  bool readFile(const String& name, String& out) override;

 private:
  FS& fs_;
};

}  // namespace gardener
