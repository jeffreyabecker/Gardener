#pragma once

#include <stdint.h>

#include <vector>

#include <Arduino.h>

#include "ui/EditorScene.h"

namespace gardener::gfx::ui {

// One entry a choice offers: the key the setting stores, the title it prints and, when the page
// wants a row of its own, the paint the band runs instead of the title. The paint is given the row
// the scene narrows for it, which is the width of the value band.
struct ChoiceItem {
  String key;
  String title;
  Paint paint;
};

// The value editor of a fixed list of choices, from choice_scene.be: Up and Down step through the
// entries and wrap at the ends, OK keeps the key showing and Cancel puts back the key it opened on.
// A nullable setting's nothing sits one entry below the first: Down at the first clears back to
// nothing, Up from nothing enters the first, and Down at nothing stays.
//
// An entry's paint is the page's own — the scene places the rows, inverts the one showing and
// scrolls the window to keep it in view, and knows nothing of what an entry draws.
class ChoiceScene : public EditorScene {
 public:
  // `current` is the key stored now. A nullable setting holding nothing passes an empty one, which
  // is what the band prints `--` for; a key no entry names shows the first, as a setting that no
  // longer offers it reads as its default.
  ChoiceScene(GestureQueue& gestures, String& label, const String& current,
              const std::vector<ChoiceItem>& items, bool nullable);

  // What the entry showing prints: its title, or `--` when the setting holds nothing.
  String text() const;

  // The key the setting stores: what OK keeps and Cancel puts back.
  String value() override;

 protected:
  void nudge(int8_t direction) override;
  Paint pageContents() override;

 private:
  int16_t index() const;

  std::vector<ChoiceItem> items_;
  bool nullable_;
};

}  // namespace gardener::gfx::ui
