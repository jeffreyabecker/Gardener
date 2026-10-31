#pragma once

#include <stdint.h>

#include <any>
#include <functional>
#include <utility>

#include <Arduino.h>

#include "layout/Drawing.h"
#include "panel/page/Scene.h"
#include "panel/page/Ux.h"

namespace gardener::panel::page {

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
  // The arrows a hint band draws for the keys: Up and Down step the value, and Back and Forward
  // move between a kind's own fields. None is the absence of a second arrow in a one-arrow item.
  enum class HintArrow : uint8_t { None, Up, Down, Back, Forward };

  void drawPage(gfx::Canvas& canvas) override;
  bool handle(const char* gesture) override;
  virtual gfx::PaintNodeRef pageComponents();
  virtual gfx::PaintNodeRef pageContents();

  virtual gfx::PaintNodeRef titleComponent();
  virtual gfx::PaintNodeRef hintComponent();

  // The items the hint band centres: the two step arrows and the words beside them, and whatever a
  // kind's own keys need between the two.
  virtual gfx::Children hintItems();

  // One item of the band: one arrow or a pair side by side, or a run of words, each as wide as the
  // ink it draws.
  static gfx::Child arrowsHint(HintArrow first, HintArrow second = HintArrow::None);
  static gfx::Child wordsHint(const char* text);

  Ux& ux_;
  String label_;

  bool closed_ = false;

  virtual void nudge(int8_t direction) = 0;

  // Hooks a kind overrides for work of its own on the way out: the kind puts back the value it
  // opened on when canceled() runs.
  virtual void accepted() {}
  virtual void canceled() {}

  virtual const char* title() { return label_.c_str(); }

  // The words the band prints after the step arrows.
  virtual const char* hint() { return "OK=SAVE"; }

 private:
  CurriedValidator validator_;
  String refusal_;
};

}  // namespace gardener::panel::page
