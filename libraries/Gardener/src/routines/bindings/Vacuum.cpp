#include "Build.h"

#if GARDENER_PANEL

#include <Arduino.h>

#include "PanelContext.h"
#include "Logger.h"
#include "Machine.h"
#include "MachineCommand.h"
#include "VacuumSystem.h"
#include "hal/Battery.h"
#include "routines/Routines.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"

namespace gardener {
namespace berry {
namespace {

int vacuumTarget(bvm* vm) {
  const MachineCommand set{MachineCommand::Op::SetTarget, (float)argReal(vm, 1)};
  context->post(set);
  be_return_nil(vm);
}

// 0 is the ambient target: the chamber is released, and the target is left there rather than
// cleared, so a page can go back to it.
int vacuumTargetOff(bvm* vm) {
  const MachineCommand off{MachineCommand::Op::SetTarget, 0.0f};
  context->post(off);
  be_return_nil(vm);
}

// The machine's own bounds, as two constants `state()` publishes: shallower than `min` is not a
// target and `floor` is the deepest worth asking for. Kept here so a page steps and restores a
// target without carrying its own copy of the bounds — and so one bound serves the editor, a
// stored value and anything else that asks.
int vacuumClamp(bvm* vm) {
  float pressureKpa = (float)argReal(vm, 1);
  if (pressureKpa < CONTROL_MIN_TARGET_KPA) {
    pressureKpa = CONTROL_MIN_TARGET_KPA;
  } else if (pressureKpa > CONTROL_FLOOR_KPA) {
    pressureKpa = CONTROL_FLOOR_KPA;
  }
  be_pushreal(vm, pressureKpa);
  be_return(vm);
}

int vacuumStart(bvm* vm) {
  if (context->vacuum().startRefusal() != nullptr) {
    be_pushbool(vm, false);
    be_return(vm);
  }
  const MachineCommand start{MachineCommand::Op::Start};
  context->post(start);
  be_pushbool(vm, true);
  be_return(vm);
}

int vacuumStop(bvm* vm) {
  const MachineCommand stop{MachineCommand::Op::Stop};
  context->post(stop);
  be_return_nil(vm);
}

int vacuumReset(bvm* vm) {
  const MachineCommand reset{MachineCommand::Op::ResetFault};
  context->post(reset);
  be_return_nil(vm);
}

int vacuumState(bvm* vm) {
  VacuumSystem& system = context->vacuum();
  const Battery& battery = context->battery();
  const int record = beginRecord(vm, "reading");
  if (record < 0) be_return_nil(vm);
  setReal(vm, record, "pressure_kpa", system.currentPressureKpa());
  setReal(vm, record, "target", system.targetPressureKpa());
  setReal(vm, record, "duty", system.pump().dutyPercent());
  setReal(vm, record, "floor", CONTROL_FLOOR_KPA);
  setReal(vm, record, "min", CONTROL_MIN_TARGET_KPA);
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
  if (be_top(vm) >= 1 && be_isstring(vm, 1) && logger != nullptr) {
    logger->info(argText(vm, 1));
  }
  be_return_nil(vm);
}

const bnfuncinfo VacuumLibrary[] = {
    {"target", vacuumTarget},   {"target_off", vacuumTargetOff}, {"clamp", vacuumClamp},
    {"start", vacuumStart},     {"stop", vacuumStop},            {"reset", vacuumReset},
    {"state", vacuumState},     {"now_ms", vacuumNowMs},         {"log", vacuumLog},
    {nullptr, nullptr},
};

}  // namespace

void installVacuumModule(bvm* vm) { installModule(vm, "vacuum", VacuumLibrary); }

}  // namespace berry
}  // namespace gardener
#endif
