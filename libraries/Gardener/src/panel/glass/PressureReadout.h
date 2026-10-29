#pragma once

#include <stdint.h>

#include "machine/VacuumSystem.h"
#include "panel/glass/Units.h"
#include "Drawing.h"

namespace gardener {


// The AutoPressureDisplay the pages drew, composed from the drawing styles: the machine's reading
// and its target in the panel's unit, both taken when the paint runs, so one paint serves every
// beat. The reading is the primary number, centred across and down `box` at `scale`; the target is
// drawn smaller in the bottom right corner until `sizeHint` reaches 1, when the two sit side by
// side at `scale`, the reading on the left. A number carries no suffix: the unit is the band's. The
// reading is `--` while the machine is not running, which is when there is no chamber reading to
// print.
gfx::ui::Paint pressureReadout(gfx::Rect box, VacuumSystem& vacuum, DisplayUnit unit,
                               float sizeHint = 0.5f, const gfx::Font* face = nullptr,
                               uint8_t scale = 2, uint8_t secondaryScale = 1);

}  // namespace gardener
