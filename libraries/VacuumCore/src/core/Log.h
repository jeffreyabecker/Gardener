#pragma once

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Serial log with a small in RAM tail that can be pulled over Bluetooth.
namespace eggplan {

struct LogState {
  char ring[1024];
  size_t used = 0;
  uint32_t dropped = 0;
  bool serialReady = false;
};

inline LogState& logState() {
  static LogState state;
  return state;
}

inline void logBegin(uint32_t baud = 115200) {
  Serial.begin(baud);
  logState().serialReady = true;
}

// Append to the tail buffer, dropping whole lines when it is full.
inline void logPush(const char* line) {
  LogState& s = logState();
  size_t len = strlen(line);
  if (len + 2 > sizeof(s.ring)) {
    len = sizeof(s.ring) - 3;
  }
  while (s.used + len + 1 > sizeof(s.ring) && s.used > 0) {
    char* firstNewline = (char*)memchr(s.ring, '\n', s.used);
    size_t drop = firstNewline ? (size_t)(firstNewline - s.ring) + 1 : s.used;
    memmove(s.ring, s.ring + drop, s.used - drop);
    s.used -= drop;
    s.dropped++;
  }
  memcpy(s.ring + s.used, line, len);
  s.used += len;
  s.ring[s.used] = '\0';
}

inline void logLine(const char* level, const char* fmt, ...) {
  char msg[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  char line[224];
  snprintf(line, sizeof(line), "[%s] %s\n", level, msg);
  if (logState().serialReady) {
    Serial.print(line);
  }
  logPush(line);
}

inline const char* logTail() { return logState().ring; }
inline uint32_t logDropped() { return logState().dropped; }

}  // namespace eggplan

#define LOGD(...) eggplan::logLine("D", __VA_ARGS__)
#define LOGI(...) eggplan::logLine("I", __VA_ARGS__)
#define LOGW(...) eggplan::logLine("W", __VA_ARGS__)
#define LOGE(...) eggplan::logLine("E", __VA_ARGS__)
