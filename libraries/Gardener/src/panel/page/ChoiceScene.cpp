#include "panel/page/ChoiceScene.h"

#include <vector>

#include "layout/Drawing.h"
#include "layout/Layout.h"
#include "panel/page/SettingsScene.h"
#include "shapes/Text.h"

namespace gardener {
namespace {

// The pitch a choice is drawn at: a ttyp0 cut's shortest line box is ten rows, so the panel's
// design cannot be drawn in one.
constexpr int16_t Row = 8;

// One row of the window: the entry's title centred, inverted across the whole of the row when it is
// the entry showing.
gfx::PaintNodeRef rowOf(const ChoiceItem* item, bool showing) {
  const char* title = item->title.c_str();
  const gfx::Size ink = gfx::measureText(rowFace(), title, 0, Row);
  gfx::PaintNodeRef content =
      gfx::aligned(gfx::sized(gfx::text(gfx::Box::whole(), title, rowFace()), ink));
  if (showing) content = gfx::inverted(std::move(content));
  return content;
}

}  // namespace

ChoiceScene::ChoiceScene(GestureQueue& gestures, Ux& ux, String& label, const String& current,
                         const std::vector<ChoiceItem>& items, bool nullable,
                         std::function<void(String& current)> forward)
    : EditorScene(gestures, ux, label), items_(items), forward_(forward), nullable_(nullable) {
  value_ = current;
  hasValue_ = !nullable_ || !current.isEmpty();
  opened_ = value_;
  hasOpened_ = hasValue_;
}

void ChoiceScene::activate() { preview(); }

// Forward on an entry showing is the page's work; with nothing showing there is no entry to hand
// it, and a page that gave none drops the key as the editor always has.
bool ChoiceScene::handle(const char* gesture) {
  if (!closed_ && forward_ && hasValue_ && Gestures::is(gesture, Gestures::ForwardSingle)) {
    preview();
    return true;
  }
  return EditorScene::handle(gesture);
}

void ChoiceScene::nudge(int8_t direction) {
  if (items_.empty()) return;
  if (nullable_) {
    if (!hasValue_) {
      if (direction > 0) {
        value_ = items_[0].key;
        hasValue_ = true;
      }
      preview();
      return;
    }
    if (direction < 0 && value_ == items_[0].key) {
      value_ = "";
      hasValue_ = false;
      preview();
      return;
    }
  }
  const int16_t count = static_cast<int16_t>(items_.size());
  const int16_t at = index();
  const int16_t next = static_cast<int16_t>(((at + direction) % count + count) % count);
  value_ = items_[next].key;
  hasValue_ = true;
  preview();
}

void ChoiceScene::preview() {
  if (forward_ && hasValue_) forward_(value_);
}

String ChoiceScene::text() const {
  if (!hasValue_ || items_.empty()) return "--";
  return items_[index()].title;
}

std::any ChoiceScene::value() const { return hasValue_ ? std::any(value_) : std::any(); }

void ChoiceScene::canceled() {
  value_ = opened_;
  hasValue_ = hasOpened_;
}

int16_t ChoiceScene::index() const {
  for (size_t i = 0; i < items_.size(); i++) {
    if (items_[i].key == value_) return static_cast<int16_t>(i);
  }
  return 0;
}

// The entries in the value band as the column of rows a list is: one Adafruit row each, the window
// scrolled to keep the entry showing in it, the block centred in the band and the row showing
// inverted. The rows are the ones the band has room for, which is why the column is built as the
// paint runs rather than before. A window shorter than the entries carries a scrollbar down the
// right of the band, the rows stopping the width of it and its gap short.
gfx::PaintNodeRef ChoiceScene::pageContents() {
  return gfx::callable([this](gfx::Canvas& canvas) {
    if (!hasValue_ || items_.empty()) {
      const gfx::Size ink = gfx::measureText(rowFace(), "--", 0, Row);
      gfx::aligned(gfx::sized(gfx::text(gfx::Box::whole(), "--", rowFace()), ink))->draw(canvas);
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
        scrolls ? static_cast<int16_t>(canvas.width() - gfx::ScrollbarColumn) : canvas.width();
    gfx::Children rows;
    for (int16_t line = 0; line < visible; line++) {
      const int16_t at = static_cast<int16_t>(first + line);
      rows.push_back(
          gfx::Child{gfx::Cell(1.0f, static_cast<float>(Row)), rowOf(&items_[at], at == selected)});
    }
    gfx::Justification centred;
    centred.along = gfx::Justify::Center;
    gfx::View plot = canvas.child(gfx::Rect(0, 0, rowsWidth, canvas.height()));
    gfx::column(1.0f, std::move(rows), centred)->draw(plot);
    if (scrolls) gfx::drawScrollbar(canvas, first, count, visible);
  });
}

}  // namespace gardener