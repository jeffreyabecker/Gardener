#pragma once

#include <stdint.h>

#include <Arduino.h>

#include "ui/Drawing.h"
#include "ui/Scene.h"
#include "ui/Ux.h"

namespace gardener::gfx::ui {

// The page every value editor is drawn in: a title band, the contents band the kind fills, and a
// hint band. Up and Down go to nudge(), and the keys close the editor.
//
// OK and Cancel pop the editor from the stack, which holds the popped scene long enough for the
// pusher's callback to read what it answered. The kind puts back the value it opened on in
// canceled(), so a cancel leaves the setting as it was.
class EditorScene : public Scene {
 public:
  EditorScene(GestureQueue& gestures, Ux& ux, String& label);

 protected:
  void drawPage(Canvas& canvas) override;
  bool handle(const char* gesture) override;
  virtual Paint pageComponents();
  virtual Paint pageContents();

  Ux& ux_;
  String label_;

  bool closed_ = false;

  virtual void nudge(int8_t direction) = 0;

  // Hooks a kind overrides for work of its own on the way out: the kind puts back the value it
  // opened on when canceled() runs, and a jingle that was auditioning stops there.
  virtual void accepted() {}
  virtual void canceled() {}

  virtual const char* title() { return label_.c_str(); }
  virtual const char* hint() { return "\x1E/\x1F  OK=SAVE"; }
};

}  // namespace gardener::gfx::ui
