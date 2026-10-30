#include <Arduino.h>

#include "core/SystemState.h"
#include "panel/parts/BuzzerPlayer.h"
#include "BindingContext.h"
#include "Module.h"
#include "Modules.h"

namespace gardener {
namespace berry {
namespace {

// The target the default program pulls to; 0 when none has been set.
int deviceTarget(bvm* vm) {
  be_pushreal(vm, bindings->systemState().deviceTargetKpa);
  be_return(vm);
}

// The sequence a routine plays at its finish, in the same notation the device's own documents use.
int devicePlaySequence(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "device.play_sequence(notes)");
  }
  const char* sequence = argText(vm, 1);
  const bool ok = panel::parts::BuzzerPlayer::validate(sequence);
  if (ok) bindings->buzzer().play(sequence);
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
  if (be_top(vm) >= 1 && be_isstring(vm, 1) && bindings != nullptr) {
    bindings->info(argText(vm, 1));
  }
  be_return_nil(vm);
}

// A pull, not a callback: the queue is drained by the routine when it is ready, so no Berry code
// runs while a native call is on the stack.
int deviceTakeGesture(bvm* vm) {
  String gesture;
  if (bindings == nullptr || !bindings->takePress(gesture)) be_return_nil(vm);
  be_pushstring(vm, gesture.c_str());
  be_return(vm);
}

const bnfuncinfo DeviceLibrary[] = {
    {"target", deviceTarget},
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
