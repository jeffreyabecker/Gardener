#pragma once

#include <stdint.h>

#include "ui/Scene.h"

namespace gardener::gfx::ui {

// The value editor of a whole-number setting: the label, the number filling the band, a bar showing
// where it sits between the bounds when the setting has both, and the hint row. Up and Down step
// the number within its bounds, OK hands it back and Cancel hands back the value it opened on.
class IntEditScene : public Scene {
 public:
  // The bounds a setting declares; either end may be absent.
  struct Bounds {
    int32_t min = 0;
    int32_t max = 0;
    bool hasMin = false;
    bool hasMax = false;
  };

  // `value` is what the editor opens on; a nullable setting holding nothing opens with `hasValue`
  // false and prints `--`. A step below one is held to one, as the declaration is.
  IntEditScene(GestureQueue& gestures, const char* label, int32_t value, bool hasValue, int32_t step,
               Bounds bounds, bool nullable);

  int32_t value() const { return value_; }
  bool hasValue() const { return hasValue_; }

  // Whether the editor is finished, and the value it hands back: the one OK took, or the one it
  // opened on when it was cancelled.
  bool closed() const { return closed_; }

 protected:
  void drawPage(Canvas& canvas) override;
  bool handle(const char* gesture) override;

 private:
  void nudge(int8_t direction);

  const char* label_;
  int32_t value_;
  bool hasValue_;
  int32_t opened_;
  bool hasOpened_;
  int32_t step_;
  Bounds bounds_;
  bool nullable_;
  bool closed_ = false;
};

}  // namespace gardener::gfx::ui
