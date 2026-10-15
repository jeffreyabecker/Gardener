#pragma once

#include <Arduino.h>

#include "core/AppContext.h"
#include "core/VacuumConfig.h"

namespace eggplan {

// Native surface handed to scripts.
//
// Berry bindings are intentionally thin: every entry point here is a plain C++
// call so the same behaviour is available to the BLE command router, the local
// UI and unit tests without a VM in the loop.
class ScriptApi {
 public:
  explicit ScriptApi(AppContext& ctx) : ctx_(ctx) {}

  // --- pump ---------------------------------------------------------------
  void pumpSet(float percent);
  void pumpStop();
  float pumpDuty() const;
  uint32_t pumpRunMs() const;

  // --- pressure -----------------------------------------------------------
  float pressureKpa() const;     // absolute, filtered
  float vacuumKpa() const;       // deficit against the ambient reference
  float vacuumPercent() const;   // 0..100
  float ambientKpa() const;
  void setAmbientKpa(float kpa);
  void captureAmbient();

  // --- closed loop --------------------------------------------------------
  void targetKpa(float kpa);
  void targetOff();
  float target() const;
  bool targetActive() const;

  // --- display ------------------------------------------------------------
  void displayClear();
  void displayLine(int line, const char* text);
  void displayBar(float fraction);
  void displayRefresh();
  void toast(const char* text, uint32_t ms);

  // --- input --------------------------------------------------------------
  bool buttonDown(int index) const;
  // 0 = timeout, 1 = click, 2 = long press, 3 = repeat
  int buttonWait(int index, int timeoutMs);

  // --- misc ---------------------------------------------------------------
  // Formats a value for display or logging without pulling a formatter into
  // every script.
  String fmt(float value, int decimals) const;
  void log(const char* message);
  uint32_t nowMs() const;
  void delay(uint32_t ms);
  const char* programName() const;
  const char* arg(int index) const;
  String args() const;
  void setProgramArgs(const String& args);
  void setProgramName(const char* name);
  bool stopRequested() const;
  void requestStop() { stopRequested_ = true; }
  void clearStop() { stopRequested_ = false; }

  // Keeps sensors, UI, BLE and the buttons serviced while a script waits.
  void setServiceHook(void (*hook)()) { serviceHook_ = hook; }
  void setRuntime(class ScriptRuntime* runtime) { runtime_ = runtime; }
  class ScriptRuntime* runtime() const { return runtime_; }

 private:
  void service() const;

  AppContext& ctx_;
  class ScriptRuntime* runtime_ = nullptr;
  void (*serviceHook_)() = nullptr;
  String programName_;
  String programArgs_;
  bool stopRequested_ = false;
};

// A script runtime executes user programs. The Berry VM is one implementation;
// builds without it get NullRuntime so the rest of the firmware still works.
class ScriptRuntime {
 public:
  virtual ~ScriptRuntime() {}
  virtual const char* name() const = 0;
  virtual bool available() const = 0;
  virtual bool begin(AppContext& ctx, ScriptApi& api) {
    (void)ctx;
    (void)api;
    return true;
  }
  virtual bool start(const char* programName, const String& source, String& error) = 0;
  virtual void stop(const char* reason) = 0;
  virtual bool running() const = 0;
  virtual const char* currentProgram() const = 0;
  virtual void update() = 0;
  virtual bool evaluate(const String& line, String& out) {
    (void)line;
    out = "eval unsupported";
    return false;
  }
  virtual const char* lastError() const { return ""; }

  void setServiceHook(void (*hook)()) { serviceHook_ = hook; }

 protected:
  void service() {
    if (serviceHook_) serviceHook_();
  }
  void (*serviceHook_)() = nullptr;
};

class NullRuntime : public ScriptRuntime {
 public:
  const char* name() const override { return "none"; }
  bool available() const override { return false; }
  bool start(const char* programName, const String& source, String& error) override;
  void stop(const char* reason) override;
  bool running() const override { return false; }
  const char* currentProgram() const override { return ""; }
  void update() override {}
};

}  // namespace eggplan
