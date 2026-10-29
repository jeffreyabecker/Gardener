#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "machine/VacuumSystem.h"
#include "Drawing.h"

namespace gardener::panel::glass {

// The four cells of the status band, each a paint the band lays out across the bottom of the glass:
// the unit's suffix, the two ways in, the run state and the battery. A cell draws only what its
// arguments say, so the panel reads the machine once per beat and hands each one its value.
gfx::Paint bandUnit(const String& suffix);
gfx::Paint bandLinks(bool serial, bool bluetooth);
// A chamber that is moving is lit for 650 of every 1000 ms counted from `nowMs`.
gfx::Paint bandState(RunState state, uint32_t nowMs);
// No gauge is a negative percent: the outline with a slash through it. A low charge shows its fill
// for every other 500 ms.
gfx::Paint bandBattery(int8_t percent, bool low, uint32_t nowMs);

}  // namespace gardener::panel::glass
