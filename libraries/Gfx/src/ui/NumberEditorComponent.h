#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "ui/Drawing.h"

namespace gardener::gfx::ui {

// The number a value editor shows and steps: what it opened on, the increment a keypress takes, the
// bounds it is held between and whether it may hold nothing.
//
// A bound the setting does not declare is -INFINITY or INFINITY, so the clamp holds the value to
// what was declared without asking whether anything was; a nullable setting that holds nothing has
// NAN as its current value, and the band prints `--` for it.
class NumberEditorComponent {
 public:
  NumberEditorComponent(float current, float step, float min, float max, bool nullable);

  // The value the keys left it, and whether it holds one at all.
  float value() const { return current_; }
  bool hasValue() const;

  // What the band prints: the number, or `--` when it holds nothing.
  String text() const;

  // Up or Down steps the value by `step` and holds it inside the bounds. A nullable setting's
  // nothing sits one step below the floor: Up from it enters the floor, Down at the floor clears
  // back to nothing, and Down at nothing stays.
  void nudge(int8_t direction);

  // The band the editor draws: the number at the value face, over a bar showing where it sits
  // between the bounds when the setting has both. The paint centres itself in the canvas it runs on
  // and keeps its own copy of the printed value, so it outlives this component.
  Paint draw() const;

 private:
  float floor() const;
  float within(float value) const;

  float current_;
  float step_;
  float min_;
  float max_;
  bool nullable_;
};

}  // namespace gardener::gfx::ui
