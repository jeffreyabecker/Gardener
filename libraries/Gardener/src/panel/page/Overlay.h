#pragma once

#include <stdint.h>

#include "core/SystemState.h"
#include "layout/Paint.h"

namespace gardener {

// The status strip across the bottom of the glass, in four columns: the unit's suffix, the serial
// link, the run state and the gauge. It is a paint over the state the panel was handed, so the
// stack sets it once and each frame draws the machine as it stands when that frame is drawn.
class Overlay : public gfx::PaintNode {
 public:
  // The rows the overlay takes across the bottom of the glass, which is the height its columns are
  // drawn for.
  static constexpr int16_t Height = 12;

  explicit Overlay(const SystemState& state) : state_(state) {}

  // The strip drawn in the rows the stack hands over: it reads the state as it draws, so a frame
  // shows one instant.
  void draw(gfx::Canvas& canvas) const override;

 private:
  const SystemState& state_;
};

}  // namespace gardener