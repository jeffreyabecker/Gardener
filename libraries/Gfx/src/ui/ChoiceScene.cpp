#include "ui/ChoiceScene.h"

#include <vector>

#include "gfx/Text.h"
#include "ui/Drawing.h"
#include "ui/Layout.h"

namespace gardener::gfx::ui {
namespace {

// One row of the built-in face, which is the pitch a choice is drawn at.
constexpr int16_t Row = 8;

// The scrollbar's column down the right of the band, the gap between it and the rows, and the paper
// kept above and below it, as the browser places its own.
constexpr int16_t BarWidth = 2;
constexpr int16_t BarGap = 2;
constexpr int16_t BarInset = 2;

// A string centred across and down the canvas it is drawn on, at the built-in face.
void drawCentred(Canvas& canvas, const char* value) {
  const shapes::TextBox ink = shapes::measureText(nullptr, value, 1);
  const int16_t x = static_cast<int16_t>((canvas.width() - ink.width) / 2);
  const int16_t y = static_cast<int16_t>((canvas.height() - ink.height) / 2);
  text(Rect(x, y, ink.width, ink.height), value, nullptr, 1)(canvas);
}

// One row of the window: the entry's own paint, or its title centred when it has none, inverted
// across the whole of the row when it is the entry showing.
Paint rowOf(const ChoiceItem* item, bool showing) {
  return [item, showing](Canvas& own) {
    if (item->paint) {
      item->paint(own);
    } else {
      drawCentred(own, item->title.c_str());
    }
    if (showing) own.invert(Rect(0, 0, own.width(), own.height()));
  };
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

// The entries in the value band as the column of rows a list is: one built-in row each, the window
// scrolled to keep the entry showing in it, the block centred in the band and the row showing
// inverted. The rows are the ones the band has room for, which is why the column is built as the
// paint runs rather than before. A window shorter than the entries carries a scrollbar down the
// right of the band, the rows stopping the width of it and its gap short.
Paint ChoiceScene::pageContents() {
  return [this](Canvas& canvas) {
    if (!hasValue_ || items_.empty()) {
      drawCentred(canvas, "--");
      return;
    }
    const int16_t selected = index();
    const int16_t count = static_cast<int16_t>(items_.size());
    int16_t visible = static_cast<int16_t>(canvas.height() / Row);
    if (visible < 1) visible = 1;
    if (visible > count) visible = count;
    const int16_t first = selected >= visible ? static_cast<int16_t>(selected - visible + 1) : 0;
    const bool scrolls = count > visible;
    const int16_t rowsWidth =
        scrolls ? static_cast<int16_t>(canvas.width() - BarWidth - BarGap) : canvas.width();
    std::vector<Child> rows;
    for (int16_t line = 0; line < visible; line++) {
      const int16_t at = static_cast<int16_t>(first + line);
      const Child row{Cell(1.0f, static_cast<float>(Row)), rowOf(&items_[at], at == selected)};
      rows.push_back(row);
    }
    Alignment centred;
    centred.vertical = Vertical::Center;
    Canvas plot = canvas.child(Rect(0, 0, rowsWidth, canvas.height()));
    column(1.0f, rows, centred)(plot);
    if (scrolls) {
      const int16_t tall = static_cast<int16_t>(canvas.height() - 2 * BarInset);
      const int16_t left = static_cast<int16_t>(canvas.width() - BarWidth);
      scrollbar(Rect(left, BarInset, BarWidth, tall), first, count, visible, true)(canvas);
    }
  };
}

}  // namespace gardener::gfx::ui
