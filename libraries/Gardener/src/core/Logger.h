#pragma once

namespace gardener {

// What every consumer holds: the two diagnostic levels. There is one sink — SerialLogger, framed
// records on the serial line — and it writes both from boot, so no consumer picks a sink and none
// checks a level of its own.
class Logger {
 public:
  virtual ~Logger() = default;

  virtual void info(const char* text) = 0;
  virtual void warn(const char* text) = 0;
};

}  // namespace gardener