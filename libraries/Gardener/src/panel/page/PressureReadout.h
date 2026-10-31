#pragma once

#include <stdint.h>

#include "core/SystemState.h"
#include "layout/Drawing.h"

namespace gardener {

// The AutoPressureDisplay the pages drew, composed from the drawing styles: the reading and its
// target in the unit the state carries, taken from the beat's state rather than from the machine at
// paint time, so one paint serves every beat. The reading is the primary number at `scale`; the
// target is drawn smaller until `sizeHint` reaches 1, when the two sit side by side at `scale`, the
// reading on the left. Below that they are two cells of a column — the reading above, the target
// against the end of the cell under it — so a reading as wide as the box cannot be drawn through
// the target. A number carries no suffix: the unit is the band's. The reading is `--` while the
// machine is not running, which is when there is no chamber reading to print.
gfx::PaintNodeRef pressureReadout(gfx::Box box, const SystemState& state, float sizeHint = 0.5f,
                                  const gfx::Font* face = nullptr, uint8_t scale = 2,
                                  uint8_t secondaryScale = 1);

}  // namespace gardener