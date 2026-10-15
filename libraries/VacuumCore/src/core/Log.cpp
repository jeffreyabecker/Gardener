#include "core/Log.h"

#include <stdio.h>
#include <string.h>

namespace eggplan {
namespace {

constexpr size_t kRingSize = 1024;

struct LogState {
  char ring[kRingSize];
  size_t used;
  uint32_t dropped;
  LogSink sink;
};

LogState& state() {
  static LogState instance = {{0}, 0, 0, nullptr};
  return instance;
}

// Drop whole lines from the front until the new line fits.
void makeRoom(LogState& s, size_t length) {
  while (s.used + length + 1 > sizeof(s.ring) && s.used > 0) {
    char* firstNewline = (char*)memchr(s.ring, '\n', s.used);
    size_t drop = firstNewline ? (size_t)(firstNewline - s.ring) + 1 : s.used;
    memmove(s.ring, s.ring + drop, s.used - drop);
    s.used -= drop;
    s.dropped++;
  }
}

}  // namespace

void logBegin(LogSink sink) { state().sink = sink; }

void logWrite(const char* level, const char* fmt, ...) {
  char message[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  char line[224];
  int length = snprintf(line, sizeof(line), "[%s] %s\n", level, message);
  if (length <= 0) return;
  size_t size = (size_t)length;
  if (size >= sizeof(line)) size = sizeof(line) - 1;

  LogState& s = state();
  if (s.sink) s.sink(line, size);

  if (size > sizeof(s.ring) - 2) size = sizeof(s.ring) - 2;
  makeRoom(s, size);
  memcpy(s.ring + s.used, line, size);
  s.used += size;
  s.ring[s.used] = '\0';
}

const char* logTail() { return state().ring; }

uint32_t logDropped() { return state().dropped; }

}  // namespace eggplan
