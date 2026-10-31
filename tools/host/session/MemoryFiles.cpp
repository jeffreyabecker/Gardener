#include "MemoryFiles.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace gardener {

namespace {

// The keys a `key=value` document holds, with comments and blank lines skipped: the one thing the
// check needs in order to see what a write changed.
std::map<std::string, std::string> keysOf(const std::string& text) {
  std::map<std::string, std::string> keys;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(start, end - start);
    start = end + 1;

    std::string trimmed;
    for (char value : line) {
      if (!isspace(static_cast<unsigned char>(value))) trimmed += value;
    }
    if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') continue;
    const size_t equals = trimmed.find('=');
    if (equals == std::string::npos) continue;
    keys[trimmed.substr(0, equals)] = trimmed.substr(equals + 1);
  }
  return keys;
}

std::string parentOf(const std::string& path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos || slash == 0 ? "/" : path.substr(0, slash);
}

std::string baseName(const std::string& path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// A path as the map holds one: no trailing slash, and the root is "/".
std::string normalised(const std::string& path) {
  std::string text = path;
  while (text.size() > 1 && text.back() == '/') text.pop_back();
  return text.empty() ? "/" : text;
}

// The prefix every path under a folder starts with.
std::string childPrefix(const std::string& folder) {
  return folder == "/" ? std::string("/") : folder + "/";
}

}  // namespace

bool MemoryFiles::load(const std::string& root) {
  namespace fs = std::filesystem;
  std::error_code error;
  if (!fs::is_directory(root, error)) return false;

  for (fs::recursive_directory_iterator entry(root, error), end; entry != end;
       entry.increment(error)) {
    if (error) return false;
    if (entry->is_directory()) continue;

    const fs::path relative = fs::relative(entry->path(), root, error);
    if (error) return false;

    std::ifstream in(entry->path(), std::ios::binary);
    if (!in) continue;
    files_["/" + relative.generic_string()] =
        std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  return true;
}

bool MemoryFiles::read(const String& path, String& out) {
  if (readObserver_) readObserver_(path);
  std::string text;
  if (!textAt(path.c_str(), text)) return false;
  out = text.c_str();
  return true;
}

bool MemoryFiles::write(const String& path, const String& text) {
  const std::string filePath = path.c_str();
  writes_++;

  std::string previous;
  textAt(filePath, previous);
  files_[filePath] = text.c_str();

  // What changed, key by key: the driver asks whether the editor persisted this setting, and a
  // rewritten document is not by itself an answer.
  const auto before = keysOf(previous);
  for (const auto& entry : keysOf(text.c_str())) {
    const auto old = before.find(entry.first);
    if (old != before.end() && old->second == entry.second) continue;
    written_.push_back(filePath + "|" + entry.first);
  }
  return true;
}

void MemoryFiles::list(const String& folder, std::vector<String>& names) {
  names.clear();

  const std::string folderPath = "/" + std::string(folder.c_str());
  std::vector<std::string> found;
  for (const auto& entry : files_) {
    if (parentOf(entry.first) != folderPath) continue;
    const std::string name = baseName(entry.first);
    if (name.size() > 3 && name.compare(name.size() - 3, 3, ".be") == 0) found.push_back(name);
  }

  std::sort(found.begin(), found.end());
  for (const std::string& name : found) names.push_back(name.c_str());
}

void MemoryFiles::entries(const String& path, std::vector<Entry>& out) {
  out.clear();

  const std::string prefix = childPrefix(normalised(path.c_str()));
  std::map<std::string, bool> found;
  for (const auto& file : files_) {
    if (file.first.compare(0, prefix.size(), prefix) != 0) continue;
    const std::string rest = file.first.substr(prefix.size());
    const size_t slash = rest.find('/');
    // A path that carries a slash below the folder is a folder's own child, not this folder's file.
    if (slash == std::string::npos) {
      found[rest] = false;
    } else {
      found[rest.substr(0, slash)] = true;
    }
  }

  // A map's order is the name order, which is the order FileLibrary sorts to.
  for (const auto& entry : found) out.push_back(Entry{entry.first.c_str(), entry.second});
}

bool MemoryFiles::exists(const String& path) {
  const std::string filePath = normalised(path.c_str());
  if (files_.find(filePath) != files_.end()) return true;
  return isDirectory(path);
}

bool MemoryFiles::isDirectory(const String& path) {
  const std::string folder = normalised(path.c_str());
  if (folder == "/") return true;
  const std::string prefix = childPrefix(folder);
  for (const auto& file : files_) {
    if (file.first.compare(0, prefix.size(), prefix) == 0) return true;
  }
  return false;
}

bool MemoryFiles::makeDirectory(const String& path) {
  // A folder is the prefix its files share, so there is nothing to keep: one is there the moment a
  // file is written under it, as the root is from the start.
  (void)path;
  return true;
}

bool MemoryFiles::remove(const String& path) {
  const std::string filePath = normalised(path.c_str());
  const auto file = files_.find(filePath);
  if (file != files_.end()) {
    files_.erase(file);
    return true;
  }

  const std::string prefix = childPrefix(filePath);
  bool removed = false;
  for (auto held = files_.begin(); held != files_.end();) {
    if (held->first.compare(0, prefix.size(), prefix) != 0) {
      ++held;
      continue;
    }
    held = files_.erase(held);
    removed = true;
  }
  return removed;
}

bool MemoryFiles::wrote(const std::string& path, const std::string& key) const {
  const std::string entry = path + "|" + key;
  return std::find(written_.begin(), written_.end(), entry) != written_.end();
}

bool MemoryFiles::textAt(const std::string& path, std::string& out) const {
  const auto file = files_.find(path);
  if (file == files_.end()) return false;
  out = file->second;
  return true;
}

}  // namespace gardener
