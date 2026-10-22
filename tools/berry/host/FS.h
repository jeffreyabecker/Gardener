#pragma once

// Arduino's FS interface, for the host: a File the caller holds, and an FS it is opened from. The
// panel library is written against this, so the check supplies an implementation rather than a
// particular filesystem. Only the members the compiled sources call are declared.

#include <Arduino.h>

#include <memory>

class FileHandle {
 public:
  virtual ~FileHandle() = default;

  virtual size_t read(uint8_t* buffer, size_t length) = 0;
  virtual size_t write(const uint8_t* buffer, size_t length) = 0;
  virtual size_t size() const = 0;
  virtual bool isDirectory() = 0;
  virtual const char* name() const = 0;
  // The next entry of a directory, or null at the end. Zero for anything that is not one.
  virtual std::shared_ptr<FileHandle> next() { return nullptr; }
  virtual void close() {}
};

class File {
 public:
  File() = default;
  explicit File(std::shared_ptr<FileHandle> handle) : handle_(std::move(handle)) {}

  explicit operator bool() const { return handle_ != nullptr; }

  size_t read(uint8_t* buffer, size_t length) {
    return handle_ ? handle_->read(buffer, length) : 0;
  }
  size_t write(const uint8_t* buffer, size_t length) {
    return handle_ ? handle_->write(buffer, length) : 0;
  }
  size_t write(uint8_t value) { return write(&value, 1); }
  size_t print(const String& text) {
    return write(reinterpret_cast<const uint8_t*>(text.c_str()), text.length());
  }
  size_t print(const char* text) {
    return write(reinterpret_cast<const uint8_t*>(text), strlen(text));
  }
  String readString() {
    String text;
    char buffer[256];
    size_t got = 0;
    while ((got = read(reinterpret_cast<uint8_t*>(buffer), sizeof buffer - 1)) > 0) {
      buffer[got] = '\0';
      text += buffer;
    }
    return text;
  }

  size_t size() const { return handle_ ? handle_->size() : 0; }
  bool isDirectory() { return handle_ ? handle_->isDirectory() : false; }
  const char* name() const { return handle_ ? handle_->name() : ""; }
  File openNextFile() {
    return handle_ ? File(handle_->next()) : File();
  }
  void close() {
    if (!handle_) return;
    handle_->close();
    handle_.reset();
  }

 private:
  std::shared_ptr<FileHandle> handle_;
};

class FS {
 public:
  virtual ~FS() = default;

  virtual File open(const char* path, const char* mode = "r") = 0;
  virtual bool exists(const char* path) = 0;
  virtual bool remove(const char* path) = 0;
  virtual bool mkdir(const char* path) = 0;
  virtual bool rmdir(const char* path) = 0;

  File open(const String& path, const char* mode = "r") { return open(path.c_str(), mode); }
  bool exists(const String& path) { return exists(path.c_str()); }
  bool remove(const String& path) { return remove(path.c_str()); }
  bool mkdir(const String& path) { return mkdir(path.c_str()); }
  bool rmdir(const String& path) { return rmdir(path.c_str()); }
};
