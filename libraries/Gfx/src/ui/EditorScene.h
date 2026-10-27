#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "ui/Drawing.h"
#include "ui/Scene.h"

namespace gardener::gfx::ui {

// The value editor of a whole-number setting: the label, the number filling the band, a bar showing
// where it sits between the bounds when the setting has both, and the hint row. Up and Down step
// the number within its bounds, OK hands it back and Cancel hands back the value it opened on.
class EditorScene : public Scene {
 public:

  // `value` is what the editor opens on; a nullable setting holding nothing opens with `hasValue`
  // false and prints `--`. A step below one is held to one, as the declaration is.
  EditorScene(GestureQueue& gestures, String& label );



 protected:
  void drawPage(Canvas& canvas) override;
  bool handle(const char* gesture) override;
  virtual Paint pageComponents();
  virtual Paint pageContents();
  
  String label_;

  bool closed_ = false;

  virtual void nudge(int8_t direction) = 0;

  // Hooks a kind overrides for work of its own on the way out: the kind puts back the value it
  // opened on when canceled() runs, and a jingle that was auditioning stops there.
  virtual void accepted() {}
  virtual void canceled() {}

  virtual const char* title(){
    return label_.c_str();
  }
  virtual const char* hint(){
    return "\x1E/\x1F  OK=SAVE";
  }
};

}  // namespace gardener::gfx::ui
