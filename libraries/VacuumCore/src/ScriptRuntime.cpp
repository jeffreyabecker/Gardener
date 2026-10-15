#include "ScriptRuntime.h"

#include <ctype.h>

#include "Buttons.h"
#include "DisplayUI.h"
#include "Log.h"
#include "PressureSensor.h"
#include "Pump.h"
#include "Util.h"
#include "VacuumController.h"

namespace eggplan {

// ---------------------------------------------------------------------------
// ScriptApi
// ---------------------------------------------------------------------------

void ScriptApi::service() const {
  if (serviceHook_) serviceHook_();
}

void ScriptApi::pumpSet(float percent) {
  if (ctx_.control) ctx_.control->setManualDuty(percent);
}

void ScriptApi::pumpStop() {
  if (ctx_.control) ctx_.control->stop();
  if (ctx_.pump) ctx_.pump->stop();
}

float ScriptApi::pumpDuty() const { return ctx_.pump ? ctx_.pump->dutyPercent() : 0.0f; }

uint32_t ScriptApi::pumpRunMs() const { return ctx_.pump ? ctx_.pump->runTimeMs() : 0; }

float ScriptApi::pressureHpa() const { return ctx_.sensor ? ctx_.sensor->hpa() : 0.0f; }

float ScriptApi::vacuumHpa() const { return ctx_.sensor ? ctx_.sensor->vacuumHpa() : 0.0f; }

float ScriptApi::vacuumPercent() const {
  return ctx_.sensor ? ctx_.sensor->vacuumPercent() : 0.0f;
}

float ScriptApi::ambientHpa() const { return ctx_.sensor ? ctx_.sensor->ambientHpa() : 0.0f; }

void ScriptApi::setAmbientHpa(float hpa) {
  if (ctx_.sensor) ctx_.sensor->setAmbientHpa(hpa);
}

void ScriptApi::captureAmbient() {
  if (ctx_.sensor) ctx_.sensor->captureAmbient();
}

void ScriptApi::targetHpa(float hpa) {
  if (ctx_.control) ctx_.control->setTarget(hpa);
}

void ScriptApi::targetOff() {
  if (ctx_.control) ctx_.control->disableTarget();
}

float ScriptApi::target() const { return ctx_.control ? ctx_.control->targetHpa() : 0.0f; }

bool ScriptApi::targetActive() const {
  return ctx_.control ? ctx_.control->targetActive() : false;
}

void ScriptApi::displayClear() {
  if (ctx_.ui) ctx_.ui->overlayClear();
}

void ScriptApi::displayLine(int line, const char* text) {
  if (ctx_.ui) ctx_.ui->overlayLine(line, text);
}

void ScriptApi::displayBar(float fraction) {
  if (ctx_.ui) ctx_.ui->overlayBar(fraction);
}

void ScriptApi::displayRefresh() {
  if (ctx_.ui) ctx_.ui->overlayRefresh();
}

void ScriptApi::toast(const char* text, uint32_t ms) {
  if (ctx_.ui) ctx_.ui->toast(text, ms);
}

bool ScriptApi::buttonDown(int index) const {
  if (!ctx_.buttons || index < 0 || index >= (int)ButtonId::Count) return false;
  return ctx_.buttons->isDown((ButtonId)index);
}

int ScriptApi::buttonWait(int index, int timeoutMs) {
  if (!ctx_.buttons || index < 0 || index >= (int)ButtonId::Count) return 0;
  ButtonEvent event = ButtonEvent::None;
  if (!ctx_.buttons->waitForEvent((ButtonId)index, event, (uint32_t)timeoutMs)) return 0;
  if (event == ButtonEvent::Click) return 1;
  if (event == ButtonEvent::LongPress) return 2;
  if (event == ButtonEvent::Repeat) return 3;
  return 0;
}

void ScriptApi::log(const char* message) { LOGI("script: %s", message ? message : ""); }

uint32_t ScriptApi::nowMs() const { return ::millis(); }

void ScriptApi::delay(uint32_t ms) {
  uint32_t start = ::millis();
  while (::millis() - start < ms) {
    if (stopRequested_) return;
    service();
    ::delay(2);
  }
}

const char* ScriptApi::programName() const { return programName_.c_str(); }

const char* ScriptApi::arg(int index) const {
  // Arguments are stored as a single whitespace separated string.
  int current = 0;
  int i = 0;
  int len = (int)programArgs_.length();
  while (i < len) {
    while (i < len && isspace((unsigned char)programArgs_[i])) i++;
    if (i >= len) break;
    int start = i;
    while (i < len && !isspace((unsigned char)programArgs_[i])) i++;
    if (current == index) {
      static String value;
      value = programArgs_.substring(start, i);
      return value.c_str();
    }
    current++;
  }
  return "";
}

String ScriptApi::args() const { return programArgs_; }

void ScriptApi::setProgramArgs(const String& args) { programArgs_ = args; }

void ScriptApi::setProgramName(const char* name) { programName_ = name ? name : ""; }

bool ScriptApi::stopRequested() const {
  if (stopRequested_) return true;
  return runtime_ && !runtime_->running();
}

// ---------------------------------------------------------------------------
// NullRuntime
// ---------------------------------------------------------------------------

bool NullRuntime::start(const char* programName, const String& source, String& error) {
  (void)programName;
  (void)source;
  error = "firmware built without the Berry runtime";
  LOGW("runtime: %s", error.c_str());
  return false;
}

void NullRuntime::stop(const char* reason) {
  (void)reason;
}

}  // namespace eggplan
