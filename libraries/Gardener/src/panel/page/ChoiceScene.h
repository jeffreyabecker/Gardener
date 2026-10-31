#pragma once

#include <stdint.h>

#include <functional>
#include <vector>

#include <Arduino.h>

#include "panel/page/EditorScene.h"
#include "storage/SettingKind.h"

namespace gardener {

// The value editor of a fixed list of choices, from choice_scene.be: Up and Down step through the
// entries and wrap at the ends, OK keeps the key showing and Cancel puts back the key it opened on.
// A nullable setting's nothing sits one entry below the first: Down at the first clears back to
// nothing, Up from nothing enters the first, and Down at nothing stays.
//
// An entry prints its title, centred in the row: the scene places the rows, inverts the one showing
// and scrolls the window to keep it in view.
//
// Forward is the page's own too: a scene handed a forward function runs it on the key showing — when
// the editor goes on top, as its entries are stepped, and on Forward — for a page whose entries do
// something beyond being picked: an audition, a preview. A scene handed none drops the key, which is
// what every page that only reads the answer does.
class ChoiceScene : public EditorScene {
 public:
  // `current` is the key stored now. A nullable setting holding nothing passes an empty one, which
  // is what the band prints `--` for; a key no entry names shows the first, as a setting that no
  // longer offers it reads as its default. `forward` is what the page runs on the key showing, when
  // the editor opens, as the entries are stepped and on Forward; nothing for a page that has no work
  // of its own there.
  ChoiceScene(GestureQueue& gestures, Ux& ux, String& label, const String& current,
              const std::vector<ChoiceItem>& items, bool nullable,
              std::function<void(String& current)> forward = nullptr);

  // The editor going on top: the entry it opens on is previewed before the first draw.
  void activate() override;

  // What the entry showing prints: its title, or `--` when the setting holds nothing.
  String text() const;

  // The key the setting stores, in the any the stack hands back; an empty one is a nullable
  // setting with no entry showing.
  std::any value() const override;
  bool nullable() const override { return nullable_; }

 protected:
  bool handle(const char* gesture) override;
  void nudge(int8_t direction) override;
  void canceled() override;
  gfx::PaintNodeRef pageContents() override;

  // The key the setting holds, and the one it opened on: a kind that shows the same ring another
  // way — the bool's switch — reads these rather than the items.
  String value_;
  String opened_;
  bool hasValue_ = false;
  bool hasOpened_ = false;

 private:
  int16_t index() const;
  // The page's work on the entry showing, when there is one and the page has work there.
  void preview();

  std::vector<ChoiceItem> items_;
  std::function<void(String& current)> forward_;
  bool nullable_;
};

}  // namespace gardener