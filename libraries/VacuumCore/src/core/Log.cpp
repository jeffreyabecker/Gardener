#include "core/Log.h"

#include <stdio.h>

namespace eggplan {
namespace {

LogSink& sink() {
  static LogSink instance = nullptr;
  return instance;
}

}  // namespace

void logBegin(LogSink target) { sink() = target; }

void logWrite(const char* level, const char* fmt, ...) {
  LogSink target = sink();
  if (!target) return;

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
  target(line, size);
}

}  // namespace eggplan
