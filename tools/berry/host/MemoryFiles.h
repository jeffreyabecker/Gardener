#pragma once

#include <map>
#include <string>
#include <vector>

#include "routines/Library.h"
#include "storage/SettingsSource.h"

namespace gardener {

// The device's documents and the interpreter's files for the check, held in memory.
//
// It is loaded from the real data directory once, so `/settings.cfg`, each routine's `.cfg`, the
// modules, the routines and the jingle library are the ones the image ships; every write afterwards
// lands here, so a run can never touch the repository. It is also the write log the driver asks
// when it checks that the settings editor persisted a value.
class MemoryFiles : public SettingsSource, public Library {
 public:
  // Copies a directory tree into the image, as the paths the firmware would see:
  // `<root>/modules/x.be` becomes `/modules/x.be`. False when the directory is not there.
  bool load(const std::string& root);

  // --- SettingsSource ------------------------------------------------------
  bool read(const String& name, String& out) override;
  bool write(const String& name, const String& text) override;

  // --- Library -------------------------------------------------------------
  void list(const String& folder, std::vector<String>& names) override;
  bool readFile(const String& name, String& out) override;

  // Whether a `key=value` line for `key` changed in `path` since the run began.
  bool wrote(const std::string& path, const std::string& key) const;

 private:
  bool textAt(const std::string& path, std::string& out) const;

  std::map<std::string, std::string> files_;
  std::vector<std::string> written_;
};

}  // namespace gardener
