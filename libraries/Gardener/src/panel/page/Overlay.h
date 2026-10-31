#pragma once

#include <stdint.h>

#include "core/SystemState.h"
#include "layout/Drawing.h"

namespace gardener::panel::page {

// The status strip across the bottom of the glass, in four equal cells: the unit's suffix, the two
// ways in, the run state and the battery. It is a paint over the state the panel was handed, so the
// stack sets it once and each frame reads the machine as it stands when that frame is drawn.
class Overlay {
 public:
  // The rows the overlay takes across the bottom of the glass, which is the height its four cells
  // are drawn for.
  static constexpr int16_t Height = 12;

  explicit Overlay(const SystemState& state) : state_(state) {}

  // The strip as one paint: it reads the state as it draws, so a frame shows one instant.
  gfx::PaintNodeRef paint() const;

 private:
  const SystemState& state_;
};

}  // namespace gardener::panel::page
