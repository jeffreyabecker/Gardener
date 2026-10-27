#include "ui/ChoiceScene.h"

#include "gfx/Shapes.h"
#include "gfx/Text.h"
#include "ui/Drawing.h"

namespace gardener::gfx::ui {
namespace {

// One row of the built-in face, which is the pitch a choice is drawn at.
constexpr int16_t Row = 8;

// A string centred across and down the canvas it is drawn on, at the built-in face.
void drawCentred(Canvas& canvas, const char* value) {
  const shapes::TextBox ink = shapes::measureText(nullptr, value, 1);
  const int16_t x = static_cast<int16_t>((canvas.width() - ink.width) / 2);
  const int16_t y = static_cast<int16_t>((canvas.height() - ink.height) / 2);
  text(Rect(x, y, ink.width, ink.height), value, nullptr, 1)(canvas);
}

}  // namespace

ChoiceScene::ChoiceScene(GestureQueue& gestures, String& label, const String& current,
                         const std::vector<ChoiceItem>& items, bool nullable)
    : EditorScene(gestures, label), items_(items), nullable_(nullable) {
  value_ = current;
  hasValue_ = !nullable_ || !current.isEmpty();
  opened_ = value_;
  hasOpened_ = hasValue_;
}

void ChoiceScene::nudge(int8_t direction) {
  if (items_.empty()) return;
  if (nullable_) {
    if (!hasValue_) {
      if (direction > 0) {
        value_ = items_[0].key;
        hasValue_ = true;
      }
      return;
    }
    if (direction < 0 && value_ == items_[0].key) {
      value_ = "";
      hasValue_ = false;
      return;
    }
  }
  const int16_t count = static_cast<int16_t>(items_.size());
  const int16_t at = index();
  const int16_t next = static_cast<int16_t>(((at + direction) % count + count) % count);
  value_ = items_[next].key;
  hasValue_ = true;
}

String ChoiceScene::value() { return value_; }

String ChoiceScene::text() const {
  if (!hasValue_ || items_.empty()) return "--";
  return items_[index()].title;
}

int16_t ChoiceScene::index() const {
  for (size_t i = 0; i < items_.size(); i++) {
    if (items_[i].key == value_) return static_cast<int16_t>(i);
  }
  return 0;
}

// The entries in the value band, one built-in row each and the block centred in the band, the
// window scrolled to keep the entry showing in it, and that entry's whole row inverted. An entry
// draws its own paint on the row; one without a paint prints its title there instead.
Paint ChoiceScene::pageContents() {
  return [this](Canvas& canvas) {
    if (!hasValue_ || items_.empty()) {
      drawCentred(canvas, "--");
      return;
    }
    const int16_t selected = index();
    int16_t visible = static_cast<int16_t>(canvas.height() / Row);
    if (visible < 1) visible = 1;
    if (visible > static_cast<int16_t>(items_.size())) {
      visible = static_cast<int16_t>(items_.size());
    }
    const int16_t first = selected >= visible ? static_cast<int16_t>(selected - visible + 1) : 0;
    const int16_t top = static_cast<int16_t>((canvas.height() - visible * Row) / 2);
    for (int16_t line = 0; line < visible; line++) {
      const int16_t y = static_cast<int16_t>(top + line * Row);
      const ChoiceItem& item = items_[first + line];
      Canvas row = canvas.child(Rect(0, y, canvas.width(), Row));
      if (item.paint) {
        item.paint(row);
      } else {
        drawCentred(row, item.title.c_str());
      }
      if (first + line == selected) canvas.invert(Rect(0, y, canvas.width(), Row));
    }
  };
}

}  // namespace gardener::gfx::ui
