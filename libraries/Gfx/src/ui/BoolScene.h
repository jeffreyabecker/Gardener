#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "ui/ChoiceScene.h"

namespace gardener::gfx::ui {

// The value editor of an on/off setting, from bool_scene.be: OFF and ON walked with Up and Down,
// and a nullable setting's nothing below OFF. The states are the choice ring's — OFF and ON as its
// two entries — so the stepping is the choice editor's, and only the look is a bool's: the states
// as the equal cells of one switch, the one showing inverted, rather than as a column of rows.
class BoolScene : public ChoiceScene {
 public:
  // `current` is the state the document keeps: "true" or "false", and an empty one for a nullable
  // setting holding nothing, which the switch shows as `--`.
  BoolScene(GestureQueue& gestures, String& label, const String& current, bool nullable);

  // The state the keys left it on, and whether it is a state at all.
  bool on() const;
  bool hasValue() const { return hasValue_; }

 protected:
  Paint pageContents() override;

 private:
  bool nullable_;
};

}  // namespace gardener::gfx::ui
