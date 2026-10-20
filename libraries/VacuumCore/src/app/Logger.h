#pragma once

namespace gardener {

// What every consumer holds: the three diagnostic levels. The two implementations are
// StreamLogger (framed records on a stream) and NilLogger (nothing). Boot picks one from the
// persisted `log` preference, and every consumer works against this abstraction, so there is no
// runtime flag for anyone to check.
class Logger {
 public:
  virtual ~Logger() = default;

  virtual void info(const char* text) = 0;
  virtual void warn(const char* text) = 0;
  virtual void error(const char* text) = 0;
};

}  // namespace gardener
