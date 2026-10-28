#pragma once

#include <stdint.h>

#include "VacuumSystem.h"
#include "panel/Units.h"
#include "ui/Drawing.h"

namespace gardener {

// Which of the two readings the box's big number is: the chamber's depth, or the target it is being
// pulled to.
enum class ReadoutOrder : uint8_t { PressureTarget, TargetPressure };

// The AutoPressureDisplay the pages drew, composed from the drawing styles: the machine's reading
// and its target in the panel's unit, both taken when the paint runs, so one paint serves every
// beat. The primary number is centred across and down `box` at `scale`; the secondary is drawn
// smaller in the corner `position` names — `tl`, `tr`, `bl` or `br` — until `sizeHint` reaches 1,
// when the two sit side by side at `scale` with the secondary on the side the position names. A
// number carries no suffix: the unit is the band's. The pressure number is `--` while the machine
// is not running, which is when there is no chamber reading to print.
gfx::ui::Paint pressureReadout(gfx::Rect box, VacuumSystem& vacuum, DisplayUnit unit,
                               ReadoutOrder order = ReadoutOrder::PressureTarget,
                               const char* position = "br", float sizeHint = 0.5f,
                               const gfx::Font* face = nullptr, uint8_t scale = 2,
                               uint8_t secondaryScale = 1);

}  // namespace gardener
