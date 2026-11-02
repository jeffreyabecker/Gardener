#pragma once

#include <stdint.h>

#include <any>
#include <functional>
#include <utility>

#include <Arduino.h>

#include "layout/Drawing.h"
#include "panel/page/Scene.h"
#include "panel/page/Ux.h"

namespace gardener {

// The check a row's declaration gave, closed over the declaration and the set in force: whether the
// value an editor is about to keep is accepted and, when it is not, the text that refuses it. This
// is the form a scene holds; the declaration's own form, which takes the declaration and the set as
// arguments, is the settings row's.
using CurriedValidator = std::function<std::pair<bool, String>(const std::any& proposed)>;

// The page every value editor is drawn in: a title band, the contents band the kind fills, and a
// hint band. Up and Down go to nudge(), and the keys close the editor.
//
// OK and Cancel pop the editor from the stack, which holds the popped scene long enough for the
// pusher's callback to read what it answered. The kind puts back the value it opened on in
// canceled(), so a cancel leaves the setting as it was.
//
// A check the row's declaration gave is asked when OK arrives: a value it refuses keeps the editor
// open with the answer's text printed in the hint band, so what the stack hands back is always a
// value the declaration accepts.
class EditorScene : public Scene {
 public:
  EditorScene(GestureQueue& gestures, Ux& ux, String& label);

  // The check OK asks before the editor closes. An editor given none accepts every value.
  void setValidator(CurriedValidator validator);

 protected:
  void drawPage(gfx::Canvas& canvas) override;
  bool handle(const char* gesture) override;

  // The page's three bands, built for the size it will be drawn in. A kind that shows as many rows
  // as the band has room for cannot answer a tree without the band's size, so the size is what a
  // page is asked with rather than something a paint works out as it draws.
  virtual gfx::PaintNodeRef pageComponents(gfx::Size page);

  // The contents band, at the size this page's contents band takes.
  virtual gfx::PaintNodeRef pageContents(gfx::Size band);

  virtual gfx::PaintNodeRef titleComponent();

  // The band the keys are explained in: one row of the two step marks and the kind's words.
  virtual gfx::PaintNodeRef hintComponent();

  Ux& ux_;
  String label_;

  bool closed_ = false;

  virtual void nudge(int8_t direction) = 0;

  // Hooks a kind overrides for work of its own on the way out: the kind puts back the value it
  // opened on when canceled() runs.
  virtual void accepted() {}
  virtual void canceled() {}

  virtual const char* title() { return label_.c_str(); }

  // The words the band prints beside the step marks, which is all a kind says about its own keys.
  virtual const char* hintText() { return "OK=SAVE"; }

  // The text a refused value answered with, which the band prints in place of the marks: a kind
  // that builds its own band asks this before it draws one.
  const String& refusal() const { return refusal_; }

 private:
  CurriedValidator validator_;
  String refusal_;
};

}  // namespace gardener