#pragma once

#include <stdint.h>

#include "core/SystemState.h"
#include "layout/Drawing.h"

namespace gardener {

// The AutoPressureDisplay the pages drew, composed from the drawing styles: the reading and its
// target in the unit the state carries, taken from the beat's state rather than from the machine at
// paint time, so one paint serves every beat. The box is divided in two: the reading takes the upper
// three fifths and the target the rest of it, each fitted to its part, so the reading is the larger
// number and one as wide as the box is narrowed by the resolver rather than drawn through the
// target. A number carries no suffix: the unit is the band's. The reading is `--` while the machine
// is not running, which is when there is no chamber reading to print.
gfx::PaintNodeRef pressureReadout(gfx::Box box, const SystemState& state,
                                  const gfx::Family* family = nullptr);

}  // namespace gardener