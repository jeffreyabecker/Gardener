#include "app/BuzzerPlayer.h"
#include "app/Tasks.h"
#include "app/routines/bindings/Module.h"
#include "app/routines/bindings/Modules.h"
#include "core/Config.h"

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
    const MachineCommand off{MachineCommand::Op::Off};
    xQueueSend(machineCommandQueue, &off, 0);
    be_return_nil(vm);
  }
  if (hz < cfg::BUZZER_MIN_HZ) hz = cfg::BUZZER_MIN_HZ;
  if (hz > cfg::BUZZER_MAX_HZ) hz = cfg::BUZZER_MAX_HZ;
  const MachineCommand tone{MachineCommand::Op::Tone, hz, (uint32_t)duration};
  xQueueSend(machineCommandQueue, &tone, 0);
  be_return_nil(vm);
}

int buzzerPlay(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "buzzer.play(sequence)");
  }
  const char* sequence = argText(vm, 1);
  const bool ok = BuzzerPlayer::validate(sequence);
  if (ok) {
    MachineCommand play{MachineCommand::Op::Play};
    play.text = sequence;
    xQueueSend(machineCommandQueue, &play, 0);
  }
  be_pushbool(vm, ok);
  be_return(vm);
}

int buzzerOff(bvm* vm) {
  const MachineCommand off{MachineCommand::Op::Off};
  xQueueSend(machineCommandQueue, &off, 0);
  be_return_nil(vm);
}

const bnfuncinfo kBuzzerLibrary[] = {
    {"tone", buzzerTone},
    {"play", buzzerPlay},
    {"off", buzzerOff},
    {nullptr, nullptr},
};

}  // namespace

void installBuzzerModule(bvm* vm) { installModule(vm, "buzzer", kBuzzerLibrary); }

}  // namespace berry
}  // namespace gardener
