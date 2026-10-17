#pragma once

namespace gardener {

// The reporting surface: three levels, with no formatting and no transport in it. Whoever owns the
// stream implements this and decides what a record looks like, so a caller with something to say
// depends on the verb rather than on the interface that happens to print it.
class Logger {
 public:
  virtual ~Logger() = default;

  virtual void info(const char* text) = 0;
  virtual void warn(const char* text) = 0;
  virtual void error(const char* text) = 0;
};

class NilLogger : public Logger {
 public:
  void info(const char* text) override {}
  void warn(const char* text) override {}
  void error(const char* text) override {}
};

}  // namespace gardener
