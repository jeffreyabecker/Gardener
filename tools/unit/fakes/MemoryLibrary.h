#pragma once

#include <map>
#include <string>
#include <vector>

#include "hal/Library.h"

namespace gardener::test {

// The store a document case owns: no directory behind it, one map of path to text, and a count of
// the writes that landed, so a case can assert that a save of five keys wrote once and a save with
// nothing changed wrote nothing.
class MemoryLibrary : public Library {
 public:
  void list(const String& folder, std::vector<String>& names) override {
    const std::string prefix = folder.c_str();
    for (const auto& entry : files_) {
      if (entry.first.compare(0, prefix.size(), prefix) != 0) continue;
      const std::string rest = entry.first.substr(prefix.size());
      if (rest.empty() || rest.find('/') != std::string::npos) continue;
      names.push_back(rest.c_str());
    }
  }

  bool read(const String& path, String& out) override {
    const auto found = files_.find(path.c_str());
    if (found == files_.end()) return false;
    out = found->second.c_str();
    return true;
  }

  bool write(const String& path, const String& text) override {
    files_[path.c_str()] = text.c_str();
    writes_++;
    return true;
  }

  // A document the case starts with, which is not counted as a write.
  void put(const char* path, const char* text) { files_[path] = text; }

  const std::string& textAt(const char* path) const {
    static const std::string empty;
    const auto found = files_.find(path);
    return found == files_.end() ? empty : found->second;
  }

  bool has(const char* path) const { return files_.find(path) != files_.end(); }
  int writes() const { return writes_; }
  void clearWrites() { writes_ = 0; }

 private:
  std::map<std::string, std::string> files_;
  int writes_ = 0;
};

}  // namespace gardener::test
