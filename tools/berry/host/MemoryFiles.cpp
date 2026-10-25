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
