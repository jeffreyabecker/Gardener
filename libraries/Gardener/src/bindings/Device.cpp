#include <Arduino.h>

#include "BindingContext.h"
#include "Module.h"
#include "Modules.h"
#include "core/SystemState.h"
#include "core/Units.h"
#include "machine/Machine.h"
#include "panel/parts/BuzzerPlayer.h"
#include "panel/settings/SystemSettings.h"
#include "storage/DeviceSettings.h"

namespace gardener {
namespace {

// The unit the panel prints pressure in, as `device.display_unit()` answers it: the wire token, the
// suffix the glass prints, the scale from a kPa depth, the two steps and the decimals the fine step
// can express. Every field comes from the row the state names on each call, so a unit the wire
// changes reads new on the next one rather than staying what a page first saw.
constexpr const char* DisplayUnitRecord = "DisplayUnit";

const bnfuncinfo DisplayUnitFields[] = {
    {"init", recordInit},    {"name", nullptr},     {"suffix", nullptr},   {"per_kpa", nullptr},
    {"whole_step", nullptr}, {"sub_step", nullptr}, {"decimals", nullptr}, {nullptr, nullptr},
};

// The unit the panel prints pressure in, filled from the published state.
int deviceDisplayUnit(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  const DisplayUnit unit = bindings->systemState().unit;
  const DisplayUnitInfo& info = displayUnitInfo(unit);
  const int record = beginRecord(vm, DisplayUnitRecord);
  setText(vm, record, "name", info.name);
  setText(vm, record, "suffix", info.suffix);
  setReal(vm, record, "per_kpa", info.perKpa);
  setReal(vm, record, "whole_step", info.wholeStep);
  setReal(vm, record, "sub_step", info.subStep);
  setInt(vm, record, "decimals", displayDecimals(unit));
  be_return(vm);
}

// The longest run the device allows, in whole seconds: the stored maximum, or the machine's own
// ceiling where the document names none. It is the grant vacuum.start() gives a run, so a page that
// times itself asks the same document the settings list edits.
int deviceMaxDuration(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  be_pushint(vm, (bint)maxRunSeconds(bindings->settings().maxDurationSeconds()));
  be_return(vm);
}

// The sequence a routine plays at its finish, in the same notation the device's own documents use.
int devicePlaySequence(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "device.play_sequence(notes)");
  }
  const char* sequence = argText(vm, 1);
  const bool ok = BuzzerPlayer::validate(sequence);
  if (ok) bindings->buzzer().play(sequence);
  be_pushbool(vm, ok);
  be_return(vm);
}

// A tone with a duration; 0 ms means until silence(). A frequency outside the machine's range is
// clamped rather than dropped, and a non-positive one is silence.
int deviceTone(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "device.tone(hz, duration_ms)");
  }

  float hz = (float)argReal(vm, 1);
  const bint duration = argInt(vm, 2);
  if (hz <= 0.0f || duration < 0) {
    bindings->buzzer().off();
    be_return_nil(vm);
  }
  if (hz < BuzzerMinHz) hz = BuzzerMinHz;
  if (hz > BuzzerMaxHz) hz = BuzzerMaxHz;
  bindings->buzzer().tone((uint16_t)hz, (uint32_t)duration);
  be_return_nil(vm);
}

// Silence, and stop whatever is playing.
int deviceSilence(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  bindings->buzzer().off();
  be_return_nil(vm);
}

// `millis()`, the clock a page times against. The device's rather than the machine's: a page that
// touches no machine call still has one.
int deviceNowMs(bvm* vm) {
  be_pushint(vm, (bint)millis());
  be_return(vm);
}

// One `[I]` diagnostic record, written when the device's log set includes `I`. What a page reports
// lands where the firmware's own lines do, which is why the sink is the panel's logger rather than
// anything of the VM's.
int deviceLog(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  if (be_top(vm) >= 1 && be_isstring(vm, 1) && bindings != nullptr) {
    bindings->info(argText(vm, 1));
  }
  be_return_nil(vm);
}

// A pull, not a callback: the queue is drained by the routine when it is ready, so no Berry code
// runs while a native call is on the stack.
int deviceTakeGesture(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  String gesture;
  if (bindings == nullptr || !bindings->takePress(gesture)) be_return_nil(vm);
  be_pushstring(vm, gesture.c_str());
  be_return(vm);
}

// The set a system settings list settled on, written back through the device's document. Answers
// whether the document was written; an unchanged set writes nothing.
int deviceSaveSettings(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  if (bindings == nullptr) {
    be_raise(vm, "runtime_error", "device.save_settings: no panel");
  }
  if (be_top(vm) < 1) {
    be_raise(vm, "type_error", "device.save_settings(values)");
  }
  SettingValues values;
  if (!takeSettingMap(vm, 1, values)) {
    be_raise(vm, "type_error", "device.save_settings: values is a map");
  }
  be_pushbool(vm, bindings->systemSettings().apply(values));
  be_return(vm);
}

const bnfuncinfo DeviceLibrary[] = {
    {"display_unit", deviceDisplayUnit},
    {"max_duration", deviceMaxDuration},
    {"save_settings", deviceSaveSettings},
    {"play_sequence", devicePlaySequence},
    {"tone", deviceTone},
    {"silence", deviceSilence},
    {"take_gesture", deviceTakeGesture},
    {"log", deviceLog},
    {"now_ms", deviceNowMs},
    {nullptr, nullptr},
};

}  // namespace

void installDeviceModule(bvm* vm, BindingContext& bindings) {
  installRecord(vm, DisplayUnitRecord, DisplayUnitFields, bindings);
  installModule(vm, "device", DeviceLibrary, bindings);
}

}  // namespace gardener