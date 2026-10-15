#pragma once

#include <Arduino.h>
#include <stdarg.h>
#include <stddef.h>

// Serial debug log plus a small in RAM tail that can be pulled over the wire.
//
// The sink is installed by App, so this header knows nothing about Serial: a build
// without a console simply never installs one.

namespace eggplan {

using LogSink = void (*)(const char* text, size_t length);

void logBegin(LogSink sink);
void logWrite(const char* level, const char* fmt, ...);
const char* logTail();
uint32_t logDropped();

}  // namespace eggplan

#define LOGD(...) eggplan::logWrite("D", __VA_ARGS__)
#define LOGI(...) eggplan::logWrite("I", __VA_ARGS__)
#define LOGW(...) eggplan::logWrite("W", __VA_ARGS__)
#define LOGE(...) eggplan::logWrite("E", __VA_ARGS__)
