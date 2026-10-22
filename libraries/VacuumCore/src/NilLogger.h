#pragma once

#include "Logger.h"

namespace gardener {

// A Logger that writes nowhere: the `log` preference off. The device boots this when the persisted
// preference says so, and every consumer keeps working against the same interface.
class NilLogger : public Logger {
 public:
  void info(const char* text) override { (void)text; }
  void warn(const char* text) override { (void)text; }
  void error(const char* text) override { (void)text; }
};

}  // namespace gardener
