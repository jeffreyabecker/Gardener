#include "MemoryFS.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {

std::string normalize(const std::string& path) {
  if (path.empty()) return "/";
  std::string out = path.front() == '/' ? path : "/" + path;
  while (out.size() > 1 && out.back() == '/') out.pop_back();
  return out;
}

std::string parentOf(const std::string& path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos || slash == 0 ? "/" : path.substr(0, slash);
}

std::string baseName(const std::string& path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// The keys a `key=value` document holds, with the comments and blank lines skipped, which is the
// one thing the check needs to see a write change.
std::map<std::string, std::string> keysOf(const std::string& text) {
  std::map<std::string, std::string> keys;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
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

}  // namespace

// A file the check holds: a cursor into the image while reading, an append to it while writing.
class HostFile : public FileHandle {
 public:
  HostFile(MemoryFS& fs, std::string path, std::string& content, bool writing)
      : fs_(fs), path_(std::move(path)), content_(&content), writing_(writing) {
    if (writing_) {
      before_ = *content_;
      content_->clear();
    }
    name_ = baseName(path_);
  }

  size_t read(uint8_t* buffer, size_t length) override {
    if (writing_ || cursor_ >= content_->size()) return 0;
    const size_t taken = std::min(length, content_->size() - cursor_);
    memcpy(buffer, content_->data() + cursor_, taken);
    cursor_ += taken;
    return taken;
  }

  size_t write(const uint8_t* buffer, size_t length) override {
    if (!writing_) return 0;
    content_->append(reinterpret_cast<const char*>(buffer), length);
    return length;
  }

  size_t size() const override { return content_->size(); }
  bool isDirectory() override { return false; }
  const char* name() const override { return name_.c_str(); }

  void close() override {
    if (writing_ && !closed_) fs_.recordWrite(path_, before_, *content_);
    closed_ = true;
  }

 private:
  MemoryFS& fs_;
  std::string path_;
  std::string* content_;
  bool writing_ = false;
  bool closed_ = false;
  size_t cursor_ = 0;
  std::string before_;
  std::string name_;
};

// A directory the check holds: the children it was opened with, handed out one at a time.
class HostDir : public FileHandle {
 public:
  HostDir(MemoryFS& fs, std::string path, std::vector<std::string> children)
      : fs_(fs), path_(std::move(path)), children_(std::move(children)), name_(baseName(path_)) {}

  size_t read(uint8_t*, size_t) override { return 0; }
  size_t write(const uint8_t*, size_t) override { return 0; }
  size_t size() const override { return 0; }
  bool isDirectory() override { return true; }
  const char* name() const override { return name_.c_str(); }

  std::shared_ptr<FileHandle> next() override {
    if (cursor_ >= children_.size()) return nullptr;
    return fs_.handle(children_[cursor_++]);
  }

 private:
  MemoryFS& fs_;
  std::string path_;
  std::vector<std::string> children_;
  size_t cursor_ = 0;
  std::string name_;
};

bool MemoryFS::load(const std::string& root) {
  namespace fs = std::filesystem;
  std::error_code error;
  if (!fs::is_directory(root, error)) return false;

  directories_.insert("/");
  for (fs::recursive_directory_iterator entry(root, error), end; entry != end;
       entry.increment(error)) {
    if (error) return false;

    const fs::path relative = fs::relative(entry->path(), root, error);
    if (error) return false;
    const std::string key = "/" + relative.generic_string();

    if (entry->is_directory()) {
      directories_.insert(key);
      continue;
    }
    std::ifstream in(entry->path(), std::ios::binary);
    if (!in) continue;
    files_[key] = std::string(std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>());
  }
  return true;
}

File MemoryFS::open(const char* path, const char* mode) {
  const std::string key = normalize(path);
  const bool writing = mode != nullptr && mode[0] == 'w';

  if (writing) {
    // A write creates the file, but not the directory it belongs in: the device's filesystem does
    // not, either.
    if (directories_.count(key) != 0 || directories_.count(parentOf(key)) == 0) return File();
    return File(std::make_shared<HostFile>(*this, key, files_[key], true));
  }
  return File(handle(key));
}

bool MemoryFS::exists(const char* path) {
  const std::string key = normalize(path);
  return files_.count(key) != 0 || directories_.count(key) != 0;
}

bool MemoryFS::remove(const char* path) { return files_.erase(normalize(path)) != 0; }

bool MemoryFS::mkdir(const char* path) {
  const std::string key = normalize(path);
  if (exists(key.c_str())) return false;
  if (directories_.count(parentOf(key)) == 0) return false;
  directories_.insert(key);
  return true;
}

bool MemoryFS::rmdir(const char* path) {
  const std::string key = normalize(path);
  if (directories_.count(key) == 0) return false;
  for (const auto& entry : files_) {
    if (parentOf(entry.first) == key) return false;
  }
  for (const auto& entry : directories_) {
    if (entry != key && parentOf(entry) == key) return false;
  }
  directories_.erase(key);
  return true;
}

bool MemoryFS::wrote(const std::string& path, const std::string& key) const {
  const std::string entry = normalize(path) + "|" + key;
  return std::find(written_.begin(), written_.end(), entry) != written_.end();
}

std::shared_ptr<FileHandle> MemoryFS::handle(const std::string& path) {
  const auto file = files_.find(path);
  if (file != files_.end()) return std::make_shared<HostFile>(*this, path, file->second, false);
  if (directories_.count(path) == 0) return nullptr;

  std::vector<std::string> children;
  for (const auto& entry : files_) {
    if (parentOf(entry.first) == path) children.push_back(entry.first);
  }
  for (const auto& entry : directories_) {
    if (entry != path && parentOf(entry) == path) children.push_back(entry);
  }
  std::sort(children.begin(), children.end());
  return std::make_shared<HostDir>(*this, path, std::move(children));
}

void MemoryFS::recordWrite(const std::string& path, const std::string& before,
                           const std::string& after) {
  const auto previous = keysOf(before);
  for (const auto& entry : keysOf(after)) {
    const auto old = previous.find(entry.first);
    if (old != previous.end() && old->second == entry.second) continue;
    written_.push_back(normalize(path) + "|" + entry.first);
  }
}
