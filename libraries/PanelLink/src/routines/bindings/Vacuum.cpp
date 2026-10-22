#include <Arduino.h>

#include "Panel.h"
#include "Logger.h"
#include "Tasks.h"
#include "VacuumSystem.h"
#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "core/Config.h"

namespace gardener {
namespace berry {
namespace {

int vacuumTarget(bvm* vm) {
  const MachineCommand set{MachineCommand::Op::SetTarget, (float)argReal(vm, 1)};
  xQueueSend(machineCommandQueue, &set, 0);
  be_return_nil(vm);
}

int vacuumTargetOff(bvm* vm) {
  const MachineCommand clear{MachineCommand::Op::ClearTarget};
  xQueueSend(machineCommandQueue, &clear, 0);
  be_return_nil(vm);
}

// The machine's own bounds, as two constants `state()` publishes: shallower than `min` is not a
// target and `floor` is the deepest worth asking for. Kept here so a page steps and restores a
// target without carrying its own copy of the bounds — and so one bound serves the editor, a
// stored value and anything else that asks.
int vacuumClamp(bvm* vm) {
  float kpa = (float)argReal(vm, 1);
  if (kpa < cfg::CONTROL_MIN_TARGET_KPA) {
    kpa = cfg::CONTROL_MIN_TARGET_KPA;
  } else if (kpa > cfg::CONTROL_FLOOR_KPA) {
    kpa = cfg::CONTROL_FLOOR_KPA;
  }
  be_pushreal(vm, kpa);
  be_return(vm);
}

int vacuumStart(bvm* vm) {
  if (gContext->vacuum().startRefusal() != nullptr) {
    be_pushbool(vm, false);
    be_return(vm);
  }
  const MachineCommand start{MachineCommand::Op::Start};
  xQueueSend(machineCommandQueue, &start, 0);
  be_pushbool(vm, true);
  be_return(vm);
}

int vacuumStop(bvm* vm) {
  const MachineCommand stop{MachineCommand::Op::Stop};
  xQueueSend(machineCommandQueue, &stop, 0);
  be_return_nil(vm);
}

int vacuumReset(bvm* vm) {
  const MachineCommand reset{MachineCommand::Op::ResetFault};
  xQueueSend(machineCommandQueue, &reset, 0);
  be_return_nil(vm);
}

int vacuumState(bvm* vm) {
  VacuumSystem& system = gContext->vacuum();
  const Battery& battery = gContext->battery();
  const int record = beginRecord(vm, "reading");
  if (record < 0) be_return_nil(vm);
  setReal(vm, record, "kpa", system.currentKpa());
  setReal(vm, record, "target", system.targetKpa());
  setReal(vm, record, "duty", system.pump().dutyPercent());
  setReal(vm, record, "floor", cfg::CONTROL_FLOOR_KPA);
  setReal(vm, record, "min", cfg::CONTROL_MIN_TARGET_KPA);
  setText(vm, record, "state", runStateName(system.runState()));
  setText(vm, record, "fault", system.faultCode());
  // -1 rather than 0 when no gauge is fitted, so "no battery" is not "flat battery".
  setInt(vm, record, "batt", battery.present() ? (bint)battery.percent() : (bint)-1);
  setFlag(vm, record, "sensor_ok",
          system.sensor().present() && system.sensor().readingValid());
  endRecord(vm);
  be_return(vm);
}

int vacuumNowMs(bvm* vm) {
  be_pushint(vm, (bint)millis());
  be_return(vm);
}

int vacuumLog(bvm* vm) {
  if (be_top(vm) >= 1 && be_isstring(vm, 1) && gLogger != nullptr) {
    gLogger->info(argText(vm, 1));
  }
  be_return_nil(vm);
}

// The device's Back hold, as a one-shot a poll can ask about: true once after the hold, then false
// again. A page with a phase machine resets it here rather than comparing the machine's state.
int vacuumStopped(bvm* vm) {
  be_pushbool(vm, gRoutines != nullptr && gRoutines->takeStopped());
  be_return(vm);
}

const bnfuncinfo kVacuumLibrary[] = {
    {"target", vacuumTarget},   {"target_off", vacuumTargetOff}, {"clamp", vacuumClamp},
    {"start", vacuumStart},     {"stop", vacuumStop},            {"reset", vacuumReset},
    {"state", vacuumState},     {"now_ms", vacuumNowMs},         {"log", vacuumLog},
    {"stopped", vacuumStopped},
    {nullptr, nullptr},
};

}  // namespace

void installVacuumModule(bvm* vm) { installModule(vm, "vacuum", kVacuumLibrary); }

}  // namespace berry
}  // namespace gardener
