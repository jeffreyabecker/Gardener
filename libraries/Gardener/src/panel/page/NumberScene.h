#pragma once

#include <stdint.h>

#include <Arduino.h>

#include <functional>

#include "panel/page/EditorScene.h"

namespace gardener::gfx::ui {

// What the band prints for a number: a setting that reads in its own units passes one and the
// scene draws what it returns instead of `%g` of the value.
using NumberFormatter = std::function<String(float)>;

// The value editor of a number: the number at the value face, over a bar showing where it sits
// between the bounds when the setting has both. Up and Down step it within those bounds, OK keeps
// it and Cancel puts back the number it opened on; a nullable setting's nothing sits one step below
// the floor, where Up enters the floor and Down clears back to nothing.
//
// A bound the setting does not declare is -INFINITY or INFINITY, so the clamp holds the value to
// what was declared without asking whether anything was, and a setting holding nothing is NAN.
class NumberScene : public EditorScene {
 public:
  NumberScene(GestureQueue& gestures, Ux& ux, String& label, float current, float step, float min,
              float max, bool nullable, NumberFormatter format = nullptr);

  // What the band prints for the number: the formatter's text, `%g` of the number when the setting
  // passed none, or `--` when it holds nothing.
  String text() const;

  // The number the keys left it on, as the float the stack hands back; an empty any is a nullable
  // setting holding nothing.
  std::any value() const override;
  bool nullable() const override { return nullable_; }

 protected:
  void nudge(int8_t direction) override;
  void canceled() override;
  Paint pageContents() override;

 private:
  bool hasValue() const;
  float floor() const;
  float within(float value) const;

  float number_;
  float opened_;
  float step_;
  float min_;
  float max_;
  bool nullable_;
  NumberFormatter format_;

  // What the band prints. The paint closes over a pointer into it, so the scene keeps the string
  // rather than a local pageContents() returns without.
  String printed_;
};

}  // namespace gardener::gfx::ui
