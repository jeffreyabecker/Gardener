#pragma once

#include <FS.h>

#include <map>
#include <set>
#include <string>
#include <vector>

// The panel's files for the check: the data image held in memory.
//
// It is loaded from the real directory once, so /modules, /routines, /settings.cfg and the jingle
// library are the ones the image ships, and every write afterwards lands here. A run therefore
// reads what the device would read and can never touch the repository. It also keeps the keys of
// every write, which is what the driver asks when it checks that the settings editor persisted a
// value.

class MemoryFS : public FS {
 public:
  // Copies a directory tree into the image, as its absolute paths: `<root>/modules/x.be` becomes
  // `/modules/x.be`. False when the directory is not there.
  bool load(const std::string& root);

  File open(const char* path, const char* mode = "r") override;
  bool exists(const char* path) override;
  bool remove(const char* path) override;
  bool mkdir(const char* path) override;
  bool rmdir(const char* path) override;

  // Whether a `key=value` line for `key` was written to `path` since the run began.
  bool wrote(const std::string& path, const std::string& key) const;

 private:
  friend class HostFile;
  friend class HostDir;

  std::shared_ptr<FileHandle> handle(const std::string& path);
  void recordWrite(const std::string& path, const std::string& before,
                   const std::string& after);

  std::map<std::string, std::string> files_;
  std::set<std::string> directories_;
  std::vector<std::string> written_;
};
