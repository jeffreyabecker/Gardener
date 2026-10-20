#include "app/AppContext.h"
#include "app/StoreBus.h"
#include "app/routines/bindings/Module.h"
#include "app/routines/bindings/Modules.h"
#include "core/Units.h"

namespace gardener {
namespace berry {
namespace {

// The settings the device keeps as itself rather than as a routine's own values. `units()` is the
// token table `core/Units.h` holds, so a page cycles exactly what `device set unit` accepts and
// carries no second list.
int deviceUnits(bvm* vm) {
  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  for (uint8_t i = 0; i < kDisplayUnitCount; i++) {
    be_pushstring(vm, kDisplayUnits[i].name);
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
  if (accepted && gStore != nullptr) gStore->setUnit(unit);
  be_pushbool(vm, accepted);
  be_return(vm);
}

// The target the default program pulls to; 0 when none has been set. The page clamps a new value to
// the machine's bounds before it writes it, and the default page clamps again when it restores one.
int deviceTarget(bvm* vm) {
  be_pushreal(vm, gContext->settings().targetKpa());
  be_return(vm);
}

int deviceSetTarget(bvm* vm) {
  if (gStore != nullptr) gStore->setTarget((float)argReal(vm, 1));
  be_return_nil(vm);
}

// The radio preference the system page edits. BleLink reads it when it starts, so the switch decides
// whether the next boot advertises.
int deviceBluetooth(bvm* vm) {
  be_pushbool(vm, gContext->settings().bluetoothEnabled());
  be_return(vm);
}

int deviceSetBluetooth(bvm* vm) {
  if (be_isbool(vm, 1) && gStore != nullptr) gStore->setBluetooth(be_tobool(vm, 1));
  be_return_nil(vm);
}

const bnfuncinfo kDeviceLibrary[] = {
    {"units", deviceUnits},
    {"set_unit", deviceSetUnit},
    {"target", deviceTarget},
    {"set_target", deviceSetTarget},
    {"bluetooth", deviceBluetooth},
    {"set_bluetooth", deviceSetBluetooth},
    {nullptr, nullptr},
};

}  // namespace

void installDeviceModule(bvm* vm) { installModule(vm, "device", kDeviceLibrary); }

}  // namespace berry
}  // namespace gardener
