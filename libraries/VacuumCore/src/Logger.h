#pragma once

namespace gardener {

// What every consumer holds: the three diagnostic levels and the runtime switch the links flip.
// There is one sink — SerialLogger, framed records on the serial line — on from boot, so no consumer
// chooses between sinks and none checks a flag of its own.
class Logger {
 public:
  virtual ~Logger() = default;

  // Whether the records are written. The `device` domain sets this when a link asks; a consumer only
  // calls the levels.
  virtual void setEnabled(bool enabled) = 0;
  virtual bool enabled() const = 0;

  virtual void info(const char* text) = 0;
  virtual void warn(const char* text) = 0;
  virtual void error(const char* text) = 0;
};

}  // namespace gardener
