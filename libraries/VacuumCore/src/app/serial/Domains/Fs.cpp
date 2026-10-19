#include "app/serial/Domains.h"

#include <Arduino.h>
#include <stdint.h>

#include "app/AppContext.h"
#include "app/Storage.h"
#include "app/serial/Domain.h"
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

void replyBadArgument(Stream& io, const String& token) {
  String reply = "err bad_argument";
  if (!token.isEmpty()) {
    reply += ' ';
    reply += token;
  }
  reply += '\n';
  io.print(reply);
}

void replyNotReady(Stream& io, const char* reason) {
  String reply = "err not_ready ";
  reply += reason;
  reply += '\n';
  io.print(reply);
}

void replyNotFound(Stream& io, const String& path) {
  String reply = "err not_found ";
  reply += path;
  reply += '\n';
  io.print(reply);
}

void replyUnsupported(Stream& io, const String& path) {
  String reply = "err unsupported path=";
  reply += path;
  reply += " reason=reserved\n";
  io.print(reply);
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
  explicit FsDomain(AppContext& app) : app_(app) {}

  const char* name() const override { return "fs"; }
  void execute(Stream& io) override;

 private:
  enum class Payload { Complete, Idle, WriteFailed };

  void list(Stream& io, const String& args);
  void read(Stream& io, const String& args);
  void write(Stream& io, const String& args);
  void makeDirectory(Stream& io, const String& args);
  void remove(Stream& io, const String& args);

  // Reads exactly `bytes` from the stream, writing to `handle` when it is not null. The optional
  // newline after the body is consumed either way, so the next command starts on a boundary.
  Payload readPayload(Stream& io, uint32_t bytes, Storage::WriteHandle* handle);
  // Consumes a body a refused header still declared. False means the reply has already been sent.
  bool discardPayload(Stream& io, uint32_t bytes);

  AppContext& app_;
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
    makeDirectory(io, rest);
  } else if (verb == "delete") {
    remove(io, rest);
  } else {
    String reply = "err unknown_verb ";
    reply += verb;
    reply += '\n';
    io.print(reply);
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

  Storage& storage = app_.storage();
  if (!storage.exists(path.c_str())) {
    replyNotFound(io, path);
    return;
  }
  if (!storage.isDirectory(path.c_str())) {
    replyNotReady(io, "not_dir");
    return;
  }

  Storage::Directory* directory = storage.openDirectory(path.c_str());
  if (directory == nullptr) {
    replyNotReady(io, "io");
    return;
  }

  // Rows go out as the cursor yields them; the blank line is the only terminator.
  io.print("ok\n");
  Storage::Entry entry;
  while (storage.nextEntry(directory, entry)) {
    io.print(entry.directory ? "dir " : "file ");
    io.print(entry.name);
    io.print('\n');
  }
  storage.closeDirectory(directory);
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

  Storage& storage = app_.storage();
  if (!storage.exists(path.c_str())) {
    replyNotFound(io, path);
    return;
  }
  if (storage.isDirectory(path.c_str())) {
    replyNotReady(io, "is_dir");
    return;
  }

  const size_t bytes = storage.size(path.c_str());
  Storage::ReadHandle* handle = storage.openRead(path.c_str());
  if (handle == nullptr) {
    replyNotReady(io, "io");
    return;
  }

  String header = "ok bytes=";
  header += String((uint32_t)bytes);
  header += '\n';
  io.print(header);

  uint8_t buffer[kCopyChunk];
  while (true) {
    const size_t got = storage.readChunk(handle, buffer, sizeof(buffer));
    if (got == 0) break;
    io.write(buffer, got);
  }
  storage.closeRead(handle);
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
    String reply = "err out_of_range field=bytes value=";
    reply += bytesText;
    reply += " max=";
    reply += String((uint32_t)cfg::FS_WRITE_MAX_BYTES);
    reply += '\n';
    io.print(reply);
    return;
  }

  if (path == "/settings.ini") {
    if (!discardPayload(io, bytes)) return;
    replyUnsupported(io, path);
    return;
  }

  Storage& storage = app_.storage();
  if (storage.isDirectory(path.c_str())) {
    if (!discardPayload(io, bytes)) return;
    replyNotReady(io, "is_dir");
    return;
  }
  if (!storage.isDirectory(parentOf(path).c_str())) {
    if (!discardPayload(io, bytes)) return;
    replyNotReady(io, "no_parent");
    return;
  }
  size_t capacity = storage.freeBytes();
  if (storage.exists(path.c_str())) capacity += storage.size(path.c_str());
  if (bytes > capacity) {
    if (!discardPayload(io, bytes)) return;
    replyNotReady(io, "no_space");
    return;
  }

  Storage::WriteHandle* handle = storage.openWrite(path.c_str());
  if (handle == nullptr) {
    if (!discardPayload(io, bytes)) return;
    replyNotReady(io, "io");
    return;
  }

  const Payload result = readPayload(io, bytes, handle);
  if (result == Payload::Idle) {
    storage.abortWrite(handle);
    replyBadArgument(io, "");
    return;
  }
  if (result == Payload::WriteFailed) {
    storage.abortWrite(handle);
    replyNotReady(io, "io");
    return;
  }
  storage.closeWrite(handle);
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

  Storage& storage = app_.storage();
  if (storage.exists(path.c_str())) {
    replyNotReady(io, "exists");
    return;
  }
  if (!storage.isDirectory(parentOf(path).c_str())) {
    replyNotReady(io, "no_parent");
    return;
  }
  if (!storage.makeDirectory(path.c_str())) {
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

  Storage& storage = app_.storage();
  if (!storage.exists(path.c_str())) {
    replyNotFound(io, path);
    return;
  }

  size_t removed = 0;
  if (!storage.remove(path.c_str(), removed)) {
    replyNotReady(io, "io");
    return;
  }
  io.print("ok\n");
}

FsDomain::Payload FsDomain::readPayload(Stream& io, uint32_t bytes, Storage::WriteHandle* handle) {
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

    if (handle == nullptr) continue;
    buffer[filled++] = (uint8_t)value;
    if (filled == sizeof(buffer)) {
      if (!app_.storage().writeChunk(handle, buffer, filled)) return Payload::WriteFailed;
      filled = 0;
    }
  }

  if (handle != nullptr && filled > 0) {
    if (!app_.storage().writeChunk(handle, buffer, filled)) return Payload::WriteFailed;
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

Domain* createFsDomain(AppContext& app) { return new FsDomain(app); }

}  // namespace gardener
