#pragma once

#include <Arduino.h>

#include "hosts/script/ScriptRuntime.h"

namespace eggplan {

// Used when the firmware is built without a VM. Everything else still works: run and
// eval report why they cannot do anything, and the local UI keeps driving the pump.
class NullRuntime : public ScriptRuntime {
 public:
  const char* name() const override { return "none"; }
  bool available() const override { return false; }
  bool start(const char* programName, const String& source, String& error) override;
  void stop(const char* reason) override;
  bool running() const override { return false; }
  const char* currentProgram() const override { return ""; }
  void update() override {}
  bool evaluate(const String& line, String& out) override;
};

}  // namespace eggplan
