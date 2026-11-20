#pragma once

#include <stdint.h>

#include "core/Formatting.h"

namespace gardener {

// The levels a record can carry, numbered as RFC 5424 numbers the syslog severities: a smaller value
// is the more severe record, so a filter keeps a record whose level is at or below its threshold.
// Only two are written today; the rest are the standard's names, so a level a part needs later does
// not have to be invented here.
enum class LogLevel : uint8_t {
  Emergency = 0,
  Alert = 1,
  Critical = 2,
  Error = 3,
  Warning = 4,
  Notice = 5,
  Info = 6,
  Debug = 7,
};

// The letter a record is framed with. Emergency and Error would both be E, so Error keeps the R of
// the standard's `err`.
inline char logLevelLetter(LogLevel level) {
  switch (level) {
    case LogLevel::Emergency:
      return 'E';
    case LogLevel::Alert:
      return 'A';
    case LogLevel::Critical:
      return 'C';
    case LogLevel::Error:
      return 'R';
    case LogLevel::Warning:
      return 'W';
    case LogLevel::Notice:
      return 'N';
    case LogLevel::Info:
      return 'I';
    case LogLevel::Debug:
      return 'D';
  }
  return '?';
}

// What every consumer holds: one record at a level, and the names a caller reaches for. There is one
// sink — SerialLogger, framed records on the serial line — and it writes from boot, so no consumer
// picks a sink and none checks a level of its own. A record is one text; two parts or more are
// formatted into that text the way print writes them, so a caller does not build a String only to
// hand over its c_str().
class Logger {
 public:
  virtual ~Logger() = default;

  virtual void log(LogLevel level, const char* text) = 0;

  template <typename First, typename Second, typename... Rest>
  void log(LogLevel level, const First& first, const Second& second, const Rest&... rest) {
    log(level, format(first, second, rest...).c_str());
  }

  void info(const char* text) { log(LogLevel::Info, text); }
  void warn(const char* text) { log(LogLevel::Warning, text); }

  template <typename First, typename Second, typename... Rest>
  void info(const First& first, const Second& second, const Rest&... rest) {
    log(LogLevel::Info, first, second, rest...);
  }

  template <typename First, typename Second, typename... Rest>
  void warn(const First& first, const Second& second, const Rest&... rest) {
    log(LogLevel::Warning, first, second, rest...);
  }
};

}  // namespace gardener