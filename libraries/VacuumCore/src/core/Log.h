#pragma once

#include <Arduino.h>
#include <stdarg.h>
#include <stddef.h>

// Debug log with a sink installed by App, so this header knows nothing about Serial: a
// build without a console simply never installs one.
//
// There is no in-RAM tail any more: the `log` command went with remote scripting, so
// the log exists to be watched on a bench, not pulled off a sealed unit.

namespace eggplan {

using LogSink = void (*)(const char* text, size_t length);

void logBegin(LogSink sink);
void logWrite(const char* level, const char* fmt, ...);

}  // namespace eggplan

#define LOGD(...) eggplan::logWrite("D", __VA_ARGS__)
#define LOGI(...) eggplan::logWrite("I", __VA_ARGS__)
#define LOGW(...) eggplan::logWrite("W", __VA_ARGS__)
#define LOGE(...) eggplan::logWrite("E", __VA_ARGS__)
