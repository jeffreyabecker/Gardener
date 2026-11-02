#pragma once

#include <stdint.h>

#include <any>

#include <Arduino.h>

#include "panel/settings/EditorScene.h"

namespace gardener {

// The value editor of an on/off setting, from bool_scene.be: OFF and ON turned over with Up and
// Down, and a nullable setting's nothing below OFF. The page is the editor's; what a switch looks
// like is its own — the states as the equal cells of one row, the state showing inverted, rather
// than a column of rows.
class BoolScene : public EditorScene {
 public:
  // `current` is the state the document keeps: "true" or "false", and an empty one for a nullable
  // setting holding nothing, which the switch shows as `--`.
  BoolScene(GestureQueue& gestures, Ux& ux, String& label, const String& current, bool nullable);

  // The state as the switch prints it: ON, OFF, or `--` for a setting that holds nothing.
  String text() const;

  // The state the keys left it on, as the bool the stack hands back; an empty any is a nullable
  // setting holding no state at all.
  std::any value() const override;
  bool nullable() const override { return nullable_; }

 protected:
  void nudge(int8_t direction) override;
  void canceled() override;
  gfx::PaintNodeRef pageContents(gfx::Size band) override;

 private:
  bool nullable_;
  bool hasState_ = false;
  bool on_ = false;
  bool openedOn_ = false;
  bool openedHasState_ = false;
};

}  // namespace gardener
