#include <Arduino.h>
#include <stdint.h>

#include <vector>

#include "core/Library.h"
#include "serial/SerialLink.h"
#include "storage/Config.h"

namespace gardener {
inline namespace serial {
namespace {

// The declared body is held whole before the store sees it, so this cap is the heap's rather than
// the filesystem's: the largest file the image ships is under 10 KiB.
constexpr uint32_t WriteMaxBytes = 64 * 1024;
// A body that stops arriving for this long is abandoned.
constexpr uint32_t WriteIdleMs = 2000;

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

// Exactly the declared body, into `out`. False when it stopped arriving, which is the only failure
// left now that nothing is written until the body is whole.
bool readPayload(Stream& io, uint32_t bytes, String& out) {
  out = "";
  uint32_t received = 0;
  uint32_t last = millis();

  while (received < bytes) {
    if (io.available() <= 0) {
      if ((uint32_t)(millis() - last) >= WriteIdleMs) return false;
      delay(1);
      continue;
    }

    const int value = io.read();
    if (value < 0) continue;
    last = millis();
    received++;
    out += (char)value;
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
  return true;
}

void list(Stream& io, const String& args, Library& files) {
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

  if (!files.exists(path)) {
    replyNotFound(io, path);
    return;
  }
  if (!files.isDirectory(path)) {
    replyNotReady(io, "not_dir");
    return;
  }

  // Rows go out as the store yields them; the blank line is the only terminator.
  io.print("ok\n");
  std::vector<Library::Entry> entries;
  files.entries(path, entries);
  for (const Library::Entry& entry : entries) {
    io.print(entry.directory ? "dir " : "file ");
    io.print(entry.name);
    io.print('\n');
  }
  io.print('\n');
}

void read(Stream& io, const String& args, Library& files) {
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

  if (!files.exists(path)) {
    replyNotFound(io, path);
    return;
  }
  if (files.isDirectory(path)) {
    replyNotReady(io, "is_dir");
    return;
  }

  String text;
  if (!files.read(path, text)) {
    replyNotReady(io, "io");
    return;
  }
  io.print("ok bytes=");
  io.print((uint32_t)text.length());
  io.print('\n');
  io.print(text);
}

void write(Stream& io, const String& args, Library& files) {
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

  const long declared = bytesText.toInt();
  if (declared < 0) {
    replyBadArgument(io, bytesText);
    return;
  }
  const uint32_t bytes = (uint32_t)declared;
  if (bytes > WriteMaxBytes) {
    io.print("err out_of_range field=bytes value=");
    io.print(bytesText);
    io.print(" max=");
    io.print((uint32_t)WriteMaxBytes);
    io.print('\n');
    return;
  }

  // A refused header still swallows the body, so the next command starts on a boundary.
  String body;
  if (path == ConfigPath) {
    readPayload(io, bytes, body);
    replyUnsupported(io, path);
    return;
  }
  if (files.isDirectory(path)) {
    readPayload(io, bytes, body);
    replyNotReady(io, "is_dir");
    return;
  }
  if (!files.isDirectory(parentOf(path))) {
    readPayload(io, bytes, body);
    replyNotReady(io, "no_parent");
    return;
  }

  if (!readPayload(io, bytes, body)) {
    replyBadArgument(io, "");
    return;
  }
  if (!files.write(path, body)) {
    replyNotReady(io, "io");
    return;
  }
  io.print("ok\n");
}

void makeDirectory(Stream& io, const String& args, Library& files) {
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

  if (files.exists(path)) {
    replyNotReady(io, "exists");
    return;
  }
  if (!files.isDirectory(parentOf(path))) {
    replyNotReady(io, "no_parent");
    return;
  }
  if (!files.makeDirectory(path)) {
    replyNotReady(io, "io");
    return;
  }
  io.print("ok\n");
}

void removePath(Stream& io, const String& args, Library& files) {
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
  if (path == ConfigPath) {
    replyUnsupported(io, path);
    return;
  }

  if (!files.exists(path)) {
    replyNotFound(io, path);
    return;
  }
  if (!files.remove(path)) {
    replyNotReady(io, "io");
    return;
  }
  io.print("ok\n");
}

}  // namespace

// The device's files over the wire, on the store the composition mounted: one path per command, and
// a declared body for `write`.
void SerialLink::executeFiles(Stream& io) {
  String line = io.readStringUntil('\n');
  line.trim();
  if (line.length() > COMMAND_LINE_MAX) {
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
    list(io, rest, parts_.files);
  } else if (verb == "read") {
    read(io, rest, parts_.files);
  } else if (verb == "write") {
    write(io, rest, parts_.files);
  } else if (verb == "mkdir") {
    makeDirectory(io, rest, parts_.files);
  } else if (verb == "delete") {
    removePath(io, rest, parts_.files);
  } else {
    io.print("err unknown_verb ");
    io.print(verb);
    io.print('\n');
  }
}

}  // namespace serial
}  // namespace gardener
