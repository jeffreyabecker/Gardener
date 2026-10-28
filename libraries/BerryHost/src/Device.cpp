#include "Build.h"

#if GARDENER_PANEL

#include <Arduino.h>

#include "panel/BuzzerPlayer.h"
#include "core/Logger.h"
#include "panel/PanelContext.h"
#include "panel/Units.h"
#include "routines/Routines.h"
#include "Module.h"
#include "Modules.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"

namespace gardener {
namespace berry {
namespace {

// The five fields a `unit` record carries, left on the stack as the record itself so a caller can
// answer one or push it onto a list.
int pushUnitRecord(bvm* vm, DisplayUnit unit) {
  const DisplayUnitInfo& info = displayUnitInfo(unit);
  const int record = beginRecord(vm, "unit");
  if (record < 0) return record;
  setText(vm, record, "name", info.name);
  setText(vm, record, "suffix", info.suffix);
  setReal(vm, record, "per_kpa", info.perKpa);
  setReal(vm, record, "whole_step", info.wholeStep);
  setInt(vm, record, "decimals", (bint)displayDecimals(unit));
  return record;
}

// The settings the device keeps as itself rather than as a routine's own values. `units()` is the
// table `panel/Units.h` holds, one `unit` record per entry, so a page cycles exactly what
// `device set unit` accepts and reads a unit's label and scale without carrying a second list.
// `unit()` is the one in force, out of that same table, which is what a page formats and steps a
// target with.
int deviceUnits(bvm* vm) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (uint8_t i = 0; i < DisplayUnitCount; i++) {
    if (pushUnitRecord(vm, static_cast<DisplayUnit>(i)) < 0) be_return_nil(vm);
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

int deviceUnit(bvm* vm) {
  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(context->config().get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  if (pushUnitRecord(vm, unit) < 0) be_return_nil(vm);
  be_return(vm);
}

int deviceSetUnit(bvm* vm) {
  DisplayUnit unit = DisplayUnit::Kpa;
  const bool accepted =
      be_isstring(vm, 1) && parseDisplayUnit(argText(vm, 1), unit);
  if (accepted) {
    context->config().set(DeviceSection, UnitKey, displayUnitInfo(unit).name);
    context->config().save();
  }
  be_pushbool(vm, accepted);
  be_return(vm);
}

// The target the default program pulls to; 0 when none has been set. The page clamps a new value to
// the machine's bounds before it writes it, and the default page clamps again when it restores one.
int deviceTarget(bvm* vm) {
  const float target = context->config().get<float>(DeviceSection, TargetKey, 0.0f);
  be_pushreal(vm, target > 0.0f ? target : 0.0f);
  be_return(vm);
}

int deviceSetTarget(bvm* vm) {
  context->config().set(DeviceSection, TargetKey, (float)argReal(vm, 1));
  context->config().save();
  be_return_nil(vm);
}

// The radio is a build fact, not a preference: the row reports whether this image has one, and
// nothing a page can call changes that.
int deviceBluetooth(bvm* vm) {
  be_pushbool(vm, GARDENER_BLUETOOTH != 0);
  be_return(vm);
}

// The two ways a controller can reach the device, as the interface owner last reported them: the
// band's serial and Bluetooth icons light while each link is up, whether or not it has control.
int deviceLinks(bvm* vm) {
  const int record = beginRecord(vm, "links");
  if (record < 0) be_return_nil(vm);
  setFlag(vm, record, "serial", context->serialConnected());
  setFlag(vm, record, "bluetooth", context->bluetoothConnected());
  be_return(vm);
}

// The sequence a routine plays at its finish, in the same notation the device's own documents use.
int devicePlaySequence(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "device.play_sequence(notes)");
  }
  const char* sequence = argText(vm, 1);
  const bool ok = BuzzerPlayer::validate(sequence);
  if (ok) context->buzzer().play(sequence);
  be_pushbool(vm, ok);
  be_return(vm);
}

// A tone with a duration; 0 ms means until silence(). A frequency outside the machine's range is
// clamped rather than dropped, and a non-positive one is silence.
int deviceTone(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "device.tone(hz, duration_ms)");
  }

  float hz = (float)argReal(vm, 1);
  const bint duration = argInt(vm, 2);
  if (hz <= 0.0f || duration < 0) {
    context->buzzer().off();
    be_return_nil(vm);
  }
  if (hz < BuzzerMinHz) hz = BuzzerMinHz;
  if (hz > BuzzerMaxHz) hz = BuzzerMaxHz;
  context->buzzer().tone((uint16_t)hz, (uint32_t)duration);
  be_return_nil(vm);
}

// Silence, and stop whatever is playing.
int deviceSilence(bvm* vm) {
  context->buzzer().off();
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
  if (be_top(vm) >= 1 && be_isstring(vm, 1) && logger != nullptr) {
    logger->info(argText(vm, 1));
  }
  be_return_nil(vm);
}

// A pull, not a callback: the queue is drained by the routine when it is ready, so no Berry code
// runs while a native call is on the stack.
int deviceTakeGesture(bvm* vm) {
  String gesture;
  if (routines == nullptr || !routines->takePress(gesture)) be_return_nil(vm);
  be_pushstring(vm, gesture.c_str());
  be_return(vm);
}

const bnfuncinfo DeviceLibrary[] = {
    {"unit", deviceUnit},
    {"units", deviceUnits},
    {"set_unit", deviceSetUnit},
    {"target", deviceTarget},
    {"set_target", deviceSetTarget},
    {"bluetooth", deviceBluetooth},
    {"links", deviceLinks},
    {"play_sequence", devicePlaySequence},
    {"tone", deviceTone},
    {"silence", deviceSilence},
    {"take_gesture", deviceTakeGesture},
    {"log", deviceLog},
    {"now_ms", deviceNowMs},
    {nullptr, nullptr},
};

}  // namespace

void installDeviceModule(bvm* vm) {
  installModule(vm, "device", DeviceLibrary);
}

}  // namespace berry
}  // namespace gardener
#endif
