#include "hosts/script/ScriptApi.h"

#include <ctype.h>
#include <string.h>

#include "core/Format.h"
#include "core/Log.h"
#include "core/Math.h"
#include "hal/Buttons.h"
#include "hal/PressureSensor.h"
#include "hal/Pump.h"
#include "hosts/script/ScriptRuntime.h"
#include "services/VacuumControl.h"

namespace eggplan {

ScriptApi::ScriptApi(Pump& pump, PressureSensor& sensor, Buttons& buttons,
                     VacuumControl& control, ScriptOverlay& overlay, UiSettings& ui)
    : pump_(pump),
      sensor_(sensor),
      buttons_(buttons),
      control_(control),
      overlay_(overlay),
      ui_(ui) {}

void ScriptApi::service() const {
  if (serviceHook_) serviceHook_();
}

void ScriptApi::pumpSet(float percent) { control_.setManualDuty(percent); }

void ScriptApi::pumpStop() {
  control_.stop();
  pump_.stop();
}

float ScriptApi::pumpDuty() const { return pump_.dutyPercent(); }

uint32_t ScriptApi::pumpRunMs() const { return pump_.runTimeMs(); }

float ScriptApi::pressureKpa() const { return sensor_.kpa(); }

float ScriptApi::vacuumKpa() const { return sensor_.vacuumKpa(); }

float ScriptApi::vacuumPercent() const { return sensor_.vacuumPercent(); }

float ScriptApi::ambientKpa() const { return sensor_.ambientKpa(); }

void ScriptApi::setAmbientKpa(float kpa) { sensor_.setAmbientKpa(kpa); }

void ScriptApi::captureAmbient() { sensor_.captureAmbient(); }

void ScriptApi::targetKpa(float kpa) { control_.setTarget(kpa); }

void ScriptApi::targetOff() { control_.disableTarget(); }

float ScriptApi::target() const { return control_.targetKpa(); }

bool ScriptApi::targetActive() const { return control_.targetActive(); }

void ScriptApi::displayClear() { overlay_.clear(); }

void ScriptApi::displayLine(int line, const char* text) { overlay_.setLine(line, text); }

void ScriptApi::displayBar(float fraction) { overlay_.bar = clampf(fraction, 0.0f, 1.0f); }

void ScriptApi::toast(const char* text, uint32_t durationMs) {
  if (!text) return;
  strncpy(ui_.toast, text, sizeof(ui_.toast) - 1);
  ui_.toast[sizeof(ui_.toast) - 1] = '\0';
  ui_.toastUntilMs = millis() + durationMs;
}

bool ScriptApi::buttonDown(int index) const {
  if (index < 0 || index >= (int)ButtonId::Count) return false;
  return buttons_.isDown((ButtonId)index);
}

int ScriptApi::buttonWait(int index, int timeoutMs) {
  if (index < 0 || index >= (int)ButtonId::Count) return 0;
  ButtonEvent event = ButtonEvent::None;
  if (!buttons_.waitForEvent((ButtonId)index, event, (uint32_t)timeoutMs)) return 0;
  switch (event) {
    case ButtonEvent::Click: return 1;
    case ButtonEvent::LongPress: return 2;
    case ButtonEvent::Repeat: return 3;
    default: return 0;
  }
}

String ScriptApi::fmt(float value, int decimals) const {
  if (decimals < 0) decimals = 0;
  if (decimals > 4) decimals = 4;
  return formatFloat(value, (uint8_t)decimals);
}

void ScriptApi::log(const char* message) {
  LOGI("script: %s", message ? message : "");
}

uint32_t ScriptApi::nowMs() const { return millis(); }

void ScriptApi::delay(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    if (stopRequested()) return;
    service();
    ::delay(2);
  }
}

const char* ScriptApi::programName() const { return programName_.c_str(); }

const char* ScriptApi::arg(int index) const {
  static String value;
  int current = 0;
  int i = 0;
  int length = (int)programArgs_.length();
  while (i < length) {
    while (i < length && isspace((unsigned char)programArgs_[i])) i++;
    if (i >= length) break;
    int start = i;
    while (i < length && !isspace((unsigned char)programArgs_[i])) i++;
    if (current == index) {
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
  return runtime_ != nullptr && !runtime_->running();
}

}  // namespace eggplan
