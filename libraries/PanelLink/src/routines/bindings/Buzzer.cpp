#include "BuzzerPlayer.h"
#include "PanelContext.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

// A routine asks for a tone with a duration; 0 ms means until off(). A frequency outside the
// machine's range is clamped rather than dropped, and a non-positive one is silence.
int buzzerTone(bvm* vm) {
  if (be_top(vm) < 2) {
    be_raise(vm, "type_error", "buzzer.tone(hz, duration_ms)");
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

int buzzerPlay(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "buzzer.play(sequence)");
  }
  const char* sequence = argText(vm, 1);
  const bool ok = BuzzerPlayer::validate(sequence);
  if (ok) {
    context->buzzer().play(sequence);
  }
  be_pushbool(vm, ok);
  be_return(vm);
}

int buzzerOff(bvm* vm) {
  context->buzzer().off();
  be_return_nil(vm);
}

const bnfuncinfo BuzzerLibrary[] = {
    {"tone", buzzerTone},
    {"play", buzzerPlay},
    {"off", buzzerOff},
    {nullptr, nullptr},
};

}  // namespace

void installBuzzerModule(bvm* vm) { installModule(vm, "buzzer", BuzzerLibrary); }

}  // namespace berry
}  // namespace gardener
