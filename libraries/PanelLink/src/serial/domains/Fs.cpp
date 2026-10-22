#include "serial/PanelDomains.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <stdint.h>

#include <vector>

#include "Panel.h"
#include "serial/Domain.h"
#include "serial/Gate.h"
#include "core/Config.h"
#include "core/Parse.h"

namespace gardener {
namespace {

constexpr size_t kCopyChunk = 512;

// The next bare token, and whether there was one. Paths are one token, so nothing here takes the
// rest of the line.
bool nextToken(String& rest, String& token) {
  rest.trim();
  if (rest.isEmpty()) return false;
  const int space = rest.indexOf(' ');
  if (space < 0) {
    token = rest;
    rest = "";
  } else {
    token = rest.substring(0, space);
    rest = rest.substring(space + 1);
  }
  return true;
}

// Absolute, `/`-separated, one token. Empty, `.` and `..` segments and a trailing slash are
// refused. `/` is a directory to list but never a file to operate on.
bool validPath(const String& path, bool allowRoot) {
  if (path.isEmpty() || path.charAt(0) != '/') return false;
  if (path.indexOf(' ') >= 0) return false;
  if (path == "/") return allowRoot;

  const int length = path.length();
  if (path.charAt(length - 1) == '/') return false;

  int start = 1;
  while (start <= length) {
    const int slash = path.indexOf('/', start);
    const int end = slash < 0 ? length : slash;
    if (end == start) return false;
    const String segment = path.substring(start, end);
    if (segment == "." || segment == "..") return false;
    if (slash < 0) break;
    start = slash + 1;
  }
  return true;
}

String parentOf(const String& path) {
  const int slash = path.lastIndexOf('/');
  return slash <= 0 ? String("/") : path.substring(0, slash);
}

bool isDirectory(const char* path) {
  File file = LittleFS.open(path);
  if (!file) return false;
  const bool directory = file.isDirectory();
  file.close();
  return directory;
}

// Removes a file, or a directory with everything under it. Deleting under an open cursor
// invalidates it, so a directory's children are collected before any of them is removed.
bool removePath(const char* path) {
  const String target = path;
  File file = LittleFS.open(target);
  if (!file) return false;

  if (!file.isDirectory()) {
    file.close();
    return LittleFS.remove(target);
  }

  std::vector<String> children;
  while (true) {
    File child = file.openNextFile();
    if (!child) break;
    String childPath = target;
    if (!childPath.endsWith("/")) childPath += '/';
    childPath += child.name();
    children.push_back(childPath);
    child.close();
  }
  file.close();
  for (const String& childPath : children) {
    if (!removePath(childPath.c_str())) return false;
  }
  return LittleFS.rmdir(target);
}

void replyBadArgument(Stream& io, const String& token) {
  io.print("err bad_argument");
  if (!token.isEmpty()) {
    io.print(' ');
    io.print(token);
  }
  io.print('\n');
}

void replyNotReady(Stream& io, const char* reason) {
  io.print("err not_ready ");
  io.print(reason);
  io.print('\n');
}

void replyNotFound(Stream& io, const String& path) {
  io.print("err not_found ");
  io.print(path);
  io.print('\n');
}

void replyUnsupported(Stream& io, const String& path) {
  io.print("err unsupported path=");
  io.print(path);
  io.print(" reason=reserved\n");
}

// Exactly one argument, or the first one at fault is left in `bad`.
bool oneArg(const String& args, String& token, String& bad) {
  String rest = args;
  if (!nextToken(rest, token)) return false;
  rest.trim();
  if (rest.isEmpty()) return true;
  nextToken(rest, bad);
  return false;
}

class FsDomain : public Domain {
 public:
  FsDomain(Panel& app, Link& link) : Domain(link), app_(app) {}

  const char* name() const override { return "fs"; }
  void execute(Stream& io) override;

 private:
  enum class Payload { Complete, Idle, WriteFailed };

  void list(Stream& io, const String& args);
  void read(Stream& io, const String& args);
  void write(Stream& io, const String& args);
  void makeDirectory(Stream& io, const String& args);
  void remove(Stream& io, const String& args);

  // Reads exactly `bytes` from the stream, writing to `file` when it is not null. The optional
  // newline after the body is consumed either way, so the next command starts on a boundary.
  Payload readPayload(Stream& io, uint32_t bytes, File* file);
  // Consumes a body a refused header still declared. False means the reply has already been sent.
  bool discardPayload(Stream& io, uint32_t bytes);

  Panel& app_;
};

void FsDomain::execute(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > cfg::COMMAND_LINE_MAX) {
    replyBadArgument(io, "");
    return;
  }

  String rest = line;
  String verb;
  if (!nextToken(rest, verb)) {
    replyBadArgument(io, "");
    return;
  }
  verb.toLowerCase();

  if (verb == "list") {
    list(io, rest);
  } else if (verb == "read") {
    read(io, rest);
  } else if (verb == "write") {
    write(io, rest);
  } else if (verb == "mkdir") {
    if (requireControl(app_, link_, io)) makeDirectory(io, rest);
  } else if (verb == "delete") {
    if (requireControl(app_, link_, io)) remove(io, rest);
  } else {
    io.print("err unknown_verb ");
    io.print(verb);
    io.print('\n');
  }
}

void FsDomain::list(Stream& io, const String& args) {
  String path;
  String bad;
  if (!oneArg(args, path, bad)) {
    replyBadArgument(io, bad);
    return;
  }
  if (!validPath(path, true)) {
    replyBadArgument(io, path);
    return;
  }

  if (!LittleFS.exists(path.c_str())) {
    replyNotFound(io, path);
    return;
  }

  File directory = LittleFS.open(path.c_str());
  if (!directory) {
    replyNotReady(io, "io");
    return;
  }
  if (!directory.isDirectory()) {
    directory.close();
    replyNotReady(io, "not_dir");
    return;
  }

  // Rows go out as the cursor yields them; the blank line is the only terminator.
  io.print("ok\n");
  while (true) {
    File entry = directory.openNextFile();
    if (!entry) break;
    io.print(entry.isDirectory() ? "dir " : "file ");
    io.print(entry.name());
    io.print('\n');
    entry.close();
  }
  directory.close();
  io.print('\n');
}

void FsDomain::read(Stream& io, const String& args) {
  String path;
  String bad;
  if (!oneArg(args, path, bad)) {
    replyBadArgument(io, bad);
    return;
  }
  if (!validPath(path, false)) {
    replyBadArgument(io, path);
    return;
  }

  if (!LittleFS.exists(path.c_str())) {
    replyNotFound(io, path);
    return;
  }

  File file = LittleFS.open(path.c_str(), "r");
  if (!file) {
    replyNotReady(io, "io");
    return;
  }
  if (file.isDirectory()) {
    file.close();
    replyNotReady(io, "is_dir");
    return;
  }

  io.print("ok bytes=");
  io.print((uint32_t)file.size());
  io.print('\n');

  uint8_t buffer[kCopyChunk];
  while (true) {
    const size_t got = file.read(buffer, sizeof(buffer));
    if (got == 0) break;
    io.write(buffer, got);
  }
  file.close();
}

void FsDomain::write(Stream& io, const String& args) {
  String rest = args;
  String path;
  String bytesText;
  if (!nextToken(rest, path) || !nextToken(rest, bytesText)) {
    replyBadArgument(io, "");
    return;
  }
  rest.trim();
  if (!rest.isEmpty()) {
    String extra;
    nextToken(rest, extra);
    replyBadArgument(io, extra);
    return;
  }
  if (!validPath(path, false)) {
    replyBadArgument(io, path);
    return;
  }

  uint32_t bytes = 0;
  if (!parseU32(bytesText.c_str(), bytes)) {
    replyBadArgument(io, bytesText);
    return;
  }
  if (bytes > cfg::FS_WRITE_MAX_BYTES) {
    io.print("err out_of_range field=bytes value=");
    io.print(bytesText);
    io.print(" max=");
    io.print((uint32_t)cfg::FS_WRITE_MAX_BYTES);
    io.print('\n');
    return;
  }

  if (!hasControl(app_, link_)) {
    // The header is valid, so its declared body still has to be consumed before the refusal: the
    // next command has to start on a boundary even when this one is not allowed to run.
    if (!discardPayload(io, bytes)) return;
    io.print("err not_controlling\n");
    return;
  }

  if (path == "/settings.ini") {
    if (!discardPayload(io, bytes)) return;
    replyUnsupported(io, path);
    return;
  }

  if (isDirectory(path.c_str())) {
    if (!discardPayload(io, bytes)) return;
    replyNotReady(io, "is_dir");
    return;
  }
  if (!isDirectory(parentOf(path).c_str())) {
    if (!discardPayload(io, bytes)) return;
    replyNotReady(io, "no_parent");
    return;
  }
  // Overwriting may reuse the room the old file holds, so its size counts as free.
  const size_t total = LittleFS.totalBytes();
  const size_t used = LittleFS.usedBytes();
  size_t capacity = used < total ? total - used : 0;
  File existing = LittleFS.open(path.c_str());
  if (existing) {
    capacity += (size_t)existing.size();
    existing.close();
  }
  if (bytes > capacity) {
    if (!discardPayload(io, bytes)) return;
    replyNotReady(io, "no_space");
    return;
  }

  File file = LittleFS.open(path.c_str(), "w");
  if (!file) {
    if (!discardPayload(io, bytes)) return;
    replyNotReady(io, "io");
    return;
  }

  const Payload result = readPayload(io, bytes, &file);
  if (result != Payload::Complete) {
    // A short or abandoned body leaves nothing behind: the partial file is removed.
    file.close();
    LittleFS.remove(path.c_str());
    if (result == Payload::Idle) {
      replyBadArgument(io, "");
    } else {
      replyNotReady(io, "io");
    }
    return;
  }
  file.close();
  io.print("ok\n");
}

void FsDomain::makeDirectory(Stream& io, const String& args) {
  String path;
  String bad;
  if (!oneArg(args, path, bad)) {
    replyBadArgument(io, bad);
    return;
  }
  if (!validPath(path, false)) {
    replyBadArgument(io, path);
    return;
  }

  if (LittleFS.exists(path.c_str())) {
    replyNotReady(io, "exists");
    return;
  }
  if (!isDirectory(parentOf(path).c_str())) {
    replyNotReady(io, "no_parent");
    return;
  }
  if (!LittleFS.mkdir(path.c_str())) {
    replyNotReady(io, "io");
    return;
  }
  io.print("ok\n");
}

void FsDomain::remove(Stream& io, const String& args) {
  String path;
  String bad;
  if (!oneArg(args, path, bad)) {
    replyBadArgument(io, bad);
    return;
  }
  if (!validPath(path, false)) {
    replyBadArgument(io, path);
    return;
  }
  if (path == "/settings.ini") {
    replyUnsupported(io, path);
    return;
  }

  if (!LittleFS.exists(path.c_str())) {
    replyNotFound(io, path);
    return;
  }
  if (!removePath(path.c_str())) {
    replyNotReady(io, "io");
    return;
  }
  io.print("ok\n");
}

FsDomain::Payload FsDomain::readPayload(Stream& io, uint32_t bytes, File* file) {
  uint8_t buffer[kCopyChunk];
  size_t filled = 0;
  uint32_t received = 0;
  uint32_t last = millis();

  while (received < bytes) {
    if (io.available() <= 0) {
      if ((uint32_t)(millis() - last) >= cfg::FS_WRITE_IDLE_MS) return Payload::Idle;
      delay(1);
      continue;
    }

    const int value = io.read();
    if (value < 0) continue;
    last = millis();
    received++;

    if (file == nullptr) continue;
    buffer[filled++] = (uint8_t)value;
    if (filled == sizeof(buffer)) {
      if (file->write(buffer, filled) != filled) return Payload::WriteFailed;
      filled = 0;
    }
  }

  if (file != nullptr && filled > 0) {
    if (file->write(buffer, filled) != filled) return Payload::WriteFailed;
  }

  // The newline is optional and may still be in flight; if it arrives later, the link ignores the
  // empty line it makes.
  if (io.available() > 0) {
    if (io.peek() == '\r') {
      io.read();
      if (io.available() > 0 && io.peek() == '\n') io.read();
    } else if (io.peek() == '\n') {
      io.read();
    }
  }
  return Payload::Complete;
}

bool FsDomain::discardPayload(Stream& io, uint32_t bytes) {
  if (readPayload(io, bytes, nullptr) == Payload::Idle) {
    replyBadArgument(io, "");
    return false;
  }
  return true;
}

}  // namespace

Domain* createFsDomain(Panel& app, Link& link) { return new FsDomain(app, link); }

}  // namespace gardener
