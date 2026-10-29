#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "machine/VacuumSystem.h"
#include "Drawing.h"

namespace gardener::panel::glass {

// The rows the overlay takes across the bottom of the glass, which is the height its four cells are
// drawn for.
constexpr int16_t OverlayHeight = 12;

// The four cells of the status overlay, left to right: the unit's suffix, the two ways in, the run
// state and the battery. A cell draws only what its arguments say, so the panel reads the machine
// once per beat and hands each one its value.
gfx::Paint overlayUnit(const String& suffix);
gfx::Paint overlayLinks(bool serial, bool bluetooth);
// A chamber that is moving is lit for 650 of every 1000 ms counted from `nowMs`.
gfx::Paint overlayState(RunState state, uint32_t nowMs);
// No gauge is a negative percent: the outline with a slash through it. A low charge shows its fill
// for every other 500 ms.
gfx::Paint overlayBattery(int8_t percent, bool low, uint32_t nowMs);

}  // namespace gardener::panel::glass
