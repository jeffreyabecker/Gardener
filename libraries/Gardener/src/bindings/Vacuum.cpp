#include "BindingContext.h"
#include "Module.h"
#include "Modules.h"
#include "core/RunState.h"
#include "core/SystemState.h"
#include "machine/Machine.h"
#include "machine/MachineCommand.h"
#include "storage/DeviceSettings.h"

namespace gardener {
namespace {

// The depth the machine pulls to, in kPa below ambient. 0 is the ambient target: the chamber is
// released rather than the target cleared, so a page can go back to the stored one.
//
// The value is held to the machine's own bounds before it is posted, 0 through the floor, because a
// page has no channel to be refused on: a depth past the floor is what the pump cannot reach, and
// one below ambient is the release. The editor's bounds are not these — vacuum.clamp keeps a value
// off the ambient target and is the one a page steps within — so this floors the top alone.
int vacuumSetTargetPressure(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  float pressureKpa = (float)argReal(vm, 1);
  // `!(>=)` rather than `<` so a value that is not a number is held with the rest, as ambient is.
  if (!(pressureKpa >= 0.0f)) pressureKpa = 0.0f;
  if (pressureKpa > ControlFloorKpa) pressureKpa = ControlFloorKpa;
  const MachineCommand set{MachineCommand::Op::SetTarget, pressureKpa};
  bindings->post(set);
  bindings->settings().setTargetKpa(pressureKpa);
  be_return_nil(vm);
}

int vacuumGetTargetPressure(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  be_pushreal(vm, bindings->systemState().targetPressureKpa);
  be_return(vm);
}

// Depth below ambient as the sensor reports it: zero while there is no reading, because the part is
// not answering or no sample has been filtered yet.
int vacuumGetCurrentPressure(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  be_pushreal(vm, bindings->systemState().currentPressureKpa);
  be_return(vm);
}

// The two bounds a target is held to, as the machine states them. Kept here so a page steps and
// restores a target without carrying its own copy of the numbers, and so one bound serves the
// editor, a stored value and anything else that asks.
int vacuumGetDeepestPressure(bvm* vm) {
  be_pushreal(vm, ControlFloorKpa);
  be_return(vm);
}

int vacuumGetShallowestPressure(bvm* vm) {
  be_pushreal(vm, ControlMinTargetKpa);
  be_return(vm);
}

int vacuumClamp(bvm* vm) {
  float pressureKpa = (float)argReal(vm, 1);
  if (pressureKpa < ControlMinTargetKpa) {
    pressureKpa = ControlMinTargetKpa;
  } else if (pressureKpa > ControlFloorKpa) {
    pressureKpa = ControlFloorKpa;
  }
  be_pushreal(vm, pressureKpa);
  be_return(vm);
}

// False when the machine would refuse: no valid reading, or a latched fault. A start while a run is
// already going is a no-op that answers true. The run is granted the device's maximum, so a page
// that times a run of its own is bounded by the same document the settings list edits.
int vacuumStart(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  if (!bindings->systemState().canStart) {
    be_pushbool(vm, false);
    be_return(vm);
  }
  const uint32_t holdSeconds = maxRunSeconds(bindings->settings().maxDurationSeconds());
  const MachineCommand start{MachineCommand::Op::Start, (float)holdSeconds};
  bindings->post(start);
  be_pushbool(vm, true);
  be_return(vm);
}

int vacuumStop(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  const MachineCommand stop{MachineCommand::Op::Stop};
  bindings->post(stop);
  be_return_nil(vm);
}

int vacuumReset(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  const MachineCommand reset{MachineCommand::Op::ResetFault};
  bindings->post(reset);
  be_return_nil(vm);
}

// The state as the one word the wire and the overlay print. A vent stops reading here when it
// ends: a release settles at IDLE and a target vent returns to PULL or HOLD.
int vacuumState(bvm* vm) {
  BindingContext* bindings = boundContext(vm);
  be_pushstring(vm, runStateName(bindings->systemState().runState));
  be_return(vm);
}

const bnfuncinfo VacuumLibrary[] = {
    {"set_target_pressure", vacuumSetTargetPressure},
    {"get_target_pressure", vacuumGetTargetPressure},
    {"get_current_pressure", vacuumGetCurrentPressure},
    {"get_deepest_pressure", vacuumGetDeepestPressure},
    {"get_shallowest_pressure", vacuumGetShallowestPressure},
    {"clamp", vacuumClamp},
    {"start", vacuumStart},
    {"stop", vacuumStop},
    {"reset", vacuumReset},
    {"state", vacuumState},
    {nullptr, nullptr},
};

}  // namespace

void installVacuumModule(bvm* vm, BindingContext& bindings) {
  installModule(vm, "vacuum", VacuumLibrary, bindings);
}

}  // namespace gardener