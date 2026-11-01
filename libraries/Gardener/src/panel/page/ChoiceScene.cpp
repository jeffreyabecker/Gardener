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

// One row of the window: the entry's title filling the row, which the list inverts when the entry
// is the one showing.
gfx::PaintNodeRef rowOf(const ChoiceItem* item) {
  return gfx::text(gfx::Box::whole(), item->title.c_str(), rowFace());
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
// inverted. The rows are the ones the band has room for, which is why the band's size is what the
// page is built with. A window shorter than the entries carries a scrollbar down the right of the
// band, the rows stopping the width of it and its gap short.
gfx::PaintNodeRef ChoiceScene::pageContents(gfx::Size band) {
  if (!hasValue_ || items_.empty()) {
    // The dashes in a row of the pitch the entries are drawn at, centred in the band: a band with
    // nothing to show prints the row a first entry would take.
    gfx::Child dashes =
        gfx::row(static_cast<float>(Row), gfx::text(gfx::Box::whole(), "--", rowFace()));
    return gfx::column(1.0f, gfx::children(std::move(dashes)),
                       gfx::Justification{gfx::Justify::Center, gfx::Justify::Center});
  }
  const int16_t selected = index();
  const int16_t count = static_cast<int16_t>(items_.size());
  int16_t visible = static_cast<int16_t>(band.height / Row);
  if (visible < 1) visible = 1;
  if (visible > count) visible = count;
  const int16_t first = selected >= visible ? static_cast<int16_t>(selected - visible + 1) : 0;
  const bool scrolls = count > visible;
  const int16_t rowsWidth =
      scrolls ? static_cast<int16_t>(band.width - gfx::ScrollbarColumn) : band.width;
  gfx::Children rows;
  for (int16_t line = 0; line < visible; line++) {
    const int16_t at = static_cast<int16_t>(first + line);
    gfx::Child row = gfx::row(static_cast<float>(Row), rowOf(&items_[at]));
    if (at == selected) row.paint = gfx::inverted(std::move(row.paint));
    rows.push_back(std::move(row));
  }
  gfx::Justification centred;
  centred.along = gfx::Justify::Center;
  gfx::Child list =
      gfx::column(static_cast<float>(rowsWidth), gfx::column(1.0f, std::move(rows), centred));
  if (!scrolls) return gfx::row(1.0f, std::move(list));
  // The bar in the band's last two columns, two rows in from the top and the bottom, which is the
  // box the shape form draws beside the settings list.
  gfx::Child bar = gfx::column(
      static_cast<float>(gfx::ScrollbarColumn),
      gfx::scrollbar(
          gfx::Box(static_cast<int16_t>(gfx::ScrollbarColumn - 2), 2, 2,
                   static_cast<int16_t>(band.height - 4)),
          first, count, visible, true));
  return gfx::row(1.0f, std::move(list), std::move(bar));
}

}  // namespace gardener