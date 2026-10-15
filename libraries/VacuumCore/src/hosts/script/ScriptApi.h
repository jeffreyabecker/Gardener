#pragma once

#include <Arduino.h>

// Native surface handed to guest code.
//
// Every entry point is plain C++ so the same behaviour is reachable from the UI, the
// command router and (later) tests without a VM in the loop. The script host owns no
// hardware: it writes the overlay and the shared UI settings instead of reaching for
// the display.

namespace eggplan {

class Buttons;
class PressureSensor;
class Pump;
class ScriptRuntime;
class VacuumControl;
struct ScriptOverlay;
struct UiSettings;

class ScriptApi {
 public:
  ScriptApi(Pump& pump, PressureSensor& sensor, Buttons& buttons, VacuumControl& control,
            ScriptOverlay& overlay, UiSettings& ui);

  // --- pump ---------------------------------------------------------------
  void pumpSet(float percent);
  void pumpStop();
  float pumpDuty() const;
  uint32_t pumpRunMs() const;

  // --- pressure, all kPa ------------------------------------------------
  float pressureKpa() const;
  float vacuumKpa() const;
  float vacuumPercent() const;
  float ambientKpa() const;
  void setAmbientKpa(float kpa);
  void captureAmbient();

  // --- closed loop --------------------------------------------------------
  void targetKpa(float kpa);
  void targetOff();
  float target() const;
  bool targetActive() const;

  // --- script overlay -----------------------------------------------------
  void displayClear();
  void displayLine(int line, const char* text);
  void displayBar(float fraction);
  void toast(const char* text, uint32_t durationMs);

  // --- input --------------------------------------------------------------
  bool buttonDown(int index) const;
  // 0 = timeout, 1 = click, 2 = long press, 3 = repeat
  int buttonWait(int index, int timeoutMs);

  // --- misc ---------------------------------------------------------------
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

  void setServiceHook(void (*hook)()) { serviceHook_ = hook; }
  void setRuntime(ScriptRuntime* runtime) { runtime_ = runtime; }
  ScriptRuntime* runtime() const { return runtime_; }

 private:
  void service() const;

  Pump& pump_;
  PressureSensor& sensor_;
  Buttons& buttons_;
  VacuumControl& control_;
  ScriptOverlay& overlay_;
  UiSettings& ui_;
  ScriptRuntime* runtime_ = nullptr;
  void (*serviceHook_)() = nullptr;
  String programName_;
  String programArgs_;
  bool stopRequested_ = false;
};

}  // namespace eggplan
