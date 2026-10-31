#pragma once

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/Library.h"

namespace gardener::test {

// The store a document case owns: one map of path to text and the folders those paths imply, and a
// count of the writes that landed, so a case can assert that a save of five keys wrote once and a
// save with nothing changed wrote nothing.
class MemoryLibrary : public Library {
 public:
  void list(const String& folder, std::vector<String>& names) override {
    String path = "/";
    path += folder;
    std::vector<Entry> found;
    entries(path, found);
    for (const Entry& entry : found) {
      if (!entry.directory && entry.name.endsWith(".be")) names.push_back(entry.name);
    }
  }

  void entries(const String& path, std::vector<Entry>& out) override {
    out.clear();
    const std::string prefix = path == "/" ? "/" : std::string(path.c_str()) + "/";
    for (const std::string& directory : directories_) {
      if (directChild(prefix, directory)) {
        out.push_back(Entry{childName(prefix, directory).c_str(), true});
      }
    }
    for (const auto& entry : files_) {
      if (directChild(prefix, entry.first)) {
        out.push_back(Entry{childName(prefix, entry.first).c_str(), false});
      }
    }
    std::sort(out.begin(), out.end(), [](const Entry& left, const Entry& right) {
      return strcmp(left.name.c_str(), right.name.c_str()) < 0;
    });
  }

  bool read(const String& path, String& out) override {
    const auto found = files_.find(path.c_str());
    if (found == files_.end()) return false;
    out = found->second.c_str();
    return true;
  }

  bool write(const String& path, const String& text) override {
    remember(path.c_str(), false);
    files_[path.c_str()] = text.c_str();
    writes_++;
    return true;
  }

  bool exists(const String& path) override {
    return isDirectory(path) || files_.find(path.c_str()) != files_.end();
  }

  bool isDirectory(const String& path) override {
    return directories_.find(path.c_str()) != directories_.end();
  }

  bool makeDirectory(const String& path) override {
    remember(path.c_str(), true);
    return true;
  }

  bool remove(const String& path) override {
    const std::string target = path.c_str();
    const auto file = files_.find(target);
    if (file != files_.end()) {
      files_.erase(file);
      return true;
    }
    if (directories_.find(target) == directories_.end()) return false;

    const std::string prefix = target + "/";
    for (auto it = files_.begin(); it != files_.end();) {
      if (it->first.compare(0, prefix.size(), prefix) == 0) {
        it = files_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = directories_.begin(); it != directories_.end();) {
      if (it->compare(0, prefix.size(), prefix) == 0 || *it == target) {
        it = directories_.erase(it);
      } else {
        ++it;
      }
    }
    return true;
  }

  // A document the case starts with, which is not counted as a write.
  void put(const char* path, const char* text) {
    remember(path, false);
    files_[path] = text;
  }

  const std::string& textAt(const char* path) const {
    static const std::string empty;
    const auto found = files_.find(path);
    return found == files_.end() ? empty : found->second;
  }

  bool has(const char* path) const { return files_.find(path) != files_.end(); }
  int writes() const { return writes_; }
  void clearWrites() { writes_ = 0; }

 private:
  static bool directChild(const std::string& prefix, const std::string& path) {
    if (path.compare(0, prefix.size(), prefix) != 0) return false;
    const std::string rest = path.substr(prefix.size());
    return !rest.empty() && rest.find('/') == std::string::npos;
  }

  static std::string childName(const std::string& prefix, const std::string& path) {
    return path.substr(prefix.size());
  }

  // The folders a path names, so a stored document is reachable by walking the store; `self` adds
  // the path itself, which a folder the case makes needs and a file does not.
  void remember(const std::string& path, bool self) {
    for (size_t slash = path.find('/', 1); slash != std::string::npos;
         slash = path.find('/', slash + 1)) {
      directories_.insert(path.substr(0, slash));
    }
    if (self) directories_.insert(path);
  }

  std::map<std::string, std::string> files_;
  std::set<std::string> directories_{"/"};
  int writes_ = 0;
};

}  // namespace gardener::test
