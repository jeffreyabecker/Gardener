#include "panel/settings/ChoiceScene.h"

#include <vector>

#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener {
namespace {

// The pitch a choice is drawn at: one line of the face the rows are drawn in, which is the default
// the composition set, so a taller design shows fewer rows rather than a cut clipped into a cell that
// does not hold it.
int16_t choicePitch() {
  const int16_t pitch = static_cast<int16_t>(gfx::shortestLine(nullptr));
  return pitch > 0 ? pitch : 1;
}

// The bar a scrolling band carries down its right: two pixels wide, two rows in from the top and the
// bottom, which is the box the shape form draws beside the settings list.
constexpr int16_t BarWidth = 2;
constexpr int16_t BarInset = 2;

// The window a list of `count` rows shows in a band this tall: the rows that fit at this pitch, the
// one it starts at so the entry showing is in them, and whether the rest are a scrollbar away.
struct Window {
  int16_t visible;
  int16_t first;
  bool scrolls;
};

Window windowOf(int16_t count, int16_t selected, int16_t height, int16_t pitch) {
  int16_t visible = static_cast<int16_t>(height / pitch);
  if (visible < 1) visible = 1;
  if (visible > count) visible = count;
  const int16_t first = selected >= visible ? static_cast<int16_t>(selected - visible + 1) : 0;
  return {visible, first, count > visible};
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

// The entries in the value band as the column of rows a list is: the window the band has room for,
// the entry showing inverted, the block centred in the band and a bar beside it for the entries that
// did not fit.
gfx::PaintNodeRef ChoiceScene::pageContents(gfx::Size band) {
  const int16_t pitch = choicePitch();

  // A band with nothing to show prints the row a first entry would take.
  if (!hasValue_ || items_.empty()) {
    gfx::Child dashes = gfx::row(static_cast<float>(pitch), gfx::text("--"));
    return gfx::column(1.0f, gfx::children(std::move(dashes)),
                       gfx::Justification{gfx::Justify::Center, gfx::Justify::Center});
  }

  const int16_t count = static_cast<int16_t>(items_.size());
  const int16_t selected = index();
  const Window window = windowOf(count, selected, band.height, pitch);

  gfx::Children rows;
  for (int16_t line = 0; line < window.visible; line++) {
    const int16_t at = static_cast<int16_t>(window.first + line);
    //items_[at].icon.isEmpty
    gfx::Child row = gfx::row(static_cast<float>(pitch), gfx::text(items_[at].title.c_str()));
    if (at == selected) row.paint = gfx::inverted(std::move(row.paint));
    rows.push_back(std::move(row));
  }

  // The rows centred down the band, and the bar in the last four columns when not all of them fit.
  const int16_t rowsWidth =
      window.scrolls ? static_cast<int16_t>(band.width - gfx::ScrollbarColumn) : band.width;
  gfx::Child list{gfx::Cell(static_cast<float>(rowsWidth), 1.0f),
                  gfx::column(1.0f, std::move(rows),
                              gfx::Justification{gfx::Justify::Center, gfx::Justify::Start})};
  if (!window.scrolls) return std::move(list.paint);
  gfx::Child bar = gfx::column(
      static_cast<float>(gfx::ScrollbarColumn),
      gfx::scrollbar(gfx::Box(static_cast<int16_t>(gfx::ScrollbarColumn - BarWidth), BarInset,
                              BarWidth, static_cast<int16_t>(band.height - 2 * BarInset)),
                     window.first, count, window.visible, true));
  return gfx::row(1.0f, std::move(list), std::move(bar));
}

}  // namespace gardener