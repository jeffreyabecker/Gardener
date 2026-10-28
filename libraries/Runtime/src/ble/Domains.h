#pragma once

#include "Build.h"

#if GARDENER_BLUETOOTH

namespace gardener {

class BleDomain;
class Machine;

// The machine's slice of the GATT table: state, pressure_kpa, target, fault, command, fw, ble_proto,
// sensor, floor, caps, hold_s, max_run_s, write_result and uptime_ms. Everything a controller
// commands and reads about the machine itself, before the panel adds anything of its own. `floor`
// is here because it bounds a target, not because it is a setting: the rig's numbers are build
// symbols and no link carries them.
BleDomain* createMachineBleDomain(Machine& machine);

}  // namespace gardener

#endif  // GARDENER_BLUETOOTH
