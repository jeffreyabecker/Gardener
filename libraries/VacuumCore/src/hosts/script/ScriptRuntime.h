#pragma once

#include <Arduino.h>

// A script runtime executes user programs. The Berry VM is one implementation and is
// the reason this is the project's single abstract class: it keeps vendored, macro
// heavy Berry headers out of hosts/commands/CommandRouter.h and lets a build without
// a VM still ship NullRuntime.

namespace eggplan {

class ScriptApi;

class ScriptRuntime {
 public:
  virtual ~ScriptRuntime() {}

  virtual const char* name() const = 0;
  virtual bool available() const = 0;
  virtual bool start(const char* programName, const String& source, String& error) = 0;
  virtual void stop(const char* reason) = 0;
  virtual bool running() const = 0;
  virtual const char* currentProgram() const = 0;
  virtual void update() = 0;
  virtual bool evaluate(const String& line, String& out) = 0;

  // Lets a runtime service the rest of the application while a script waits.
  void setServiceHook(void (*hook)()) { serviceHook_ = hook; }

 protected:
  void service() {
    if (serviceHook_) serviceHook_();
  }
  void (*serviceHook_)() = nullptr;
};

}  // namespace eggplan
