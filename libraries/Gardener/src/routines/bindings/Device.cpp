#include "Build.h"

#if GARDENER_PANEL

#include "PanelContext.h"

#include "Build.h"
#include "panel/Units.h"
#include "routines/bindings/Module.h"
#include "routines/bindings/Modules.h"
#include "storage/Jingles.h"

namespace gardener {
namespace berry {
namespace {

// The settings the device keeps as itself rather than as a routine's own values. `units()` is the
// table `panel/Units.h` holds, one `unit` record per entry, so a page cycles exactly what
// `device set unit` accepts and reads a unit's label and scale without carrying a second list.
int deviceUnits(bvm* vm) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (uint8_t i = 0; i < DisplayUnitCount; i++) {
    const DisplayUnit unit = static_cast<DisplayUnit>(i);
    const DisplayUnitInfo& info = displayUnitInfo(unit);
    const int record = beginRecord(vm, "unit");
    if (record < 0) be_return_nil(vm);
    setText(vm, record, "name", info.name);
    setText(vm, record, "suffix", info.suffix);
    setReal(vm, record, "per_kpa", info.perKpa);
    setReal(vm, record, "whole_step", info.wholeStep);
    setInt(vm, record, "decimals", (bint)displayDecimals(unit));
    be_data_push(vm, list);
    be_pop(vm, 1);
  }
  be_pop(vm, 1);
  be_return(vm);
}

int deviceSetUnit(bvm* vm) {
  DisplayUnit unit = DisplayUnit::Kpa;
  const bool accepted =
      be_isstring(vm, 1) && parseDisplayUnit(argText(vm, 1), unit);
  if (accepted) context->setUnit(unit);
  be_pushbool(vm, accepted);
  be_return(vm);
}

// The target the default program pulls to; 0 when none has been set. The page clamps a new value to
// the machine's bounds before it writes it, and the default page clamps again when it restores one.
int deviceTarget(bvm* vm) {
  be_pushreal(vm, context->targetPressureKpa());
  be_return(vm);
}

int deviceSetTarget(bvm* vm) {
  context->setTarget((float)argReal(vm, 1));
  be_return_nil(vm);
}

// The radio is a build fact, not a preference: the row reports whether this image has one, and
// nothing a page can call changes that.
int deviceBluetooth(bvm* vm) {
  be_pushbool(vm, GARDENER_BLUETOOTH != 0);
  be_return(vm);
}

// The selected jingle's identifier, or "" when none has been chosen. The jingles module resolves an
// empty or unknown value to its first entry; the system page writes one of the identifiers it read
// from that list.
int deviceJingle(bvm* vm) {
  be_pushstring(vm, context->jingles().currentJingleKey().c_str());
  be_return(vm);
}

int deviceSetJingle(bvm* vm) {
  if (!be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "device.set_jingle(id)");
  }
  context->jingles().setCurrentJingle(String(argText(vm, 1)));
  be_return_nil(vm);
}

const bnfuncinfo DeviceLibrary[] = {
    {"units", deviceUnits},
    {"set_unit", deviceSetUnit},
    {"target", deviceTarget},
    {"set_target", deviceSetTarget},
    {"bluetooth", deviceBluetooth},
    {"jingle", deviceJingle},
    {"set_jingle", deviceSetJingle},
    {nullptr, nullptr},
};

}  // namespace

void installDeviceModule(bvm* vm) { installModule(vm, "device", DeviceLibrary); }

}  // namespace berry
}  // namespace gardener
#endif
