#include "panel/settings/ChoiceScene.h"

#include <utility>
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

// The rows of the list: one per entry, its key as the value the keys select it by and its title as
// the line drawn across the row.
std::vector<RowList::Row> rowsOf(const std::vector<ChoiceItem>& items) {
  std::vector<RowList::Row> rows;
  rows.reserve(items.size());
  for (const ChoiceItem& item : items)
    rows.push_back(RowList::textRow(item.key, item.title));
  return rows;
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
  rebuild();
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

// Up and Down step the entry showing through the list, which rings; a nullable setting's nothing
// sits below the first entry, so Down at the first clears it and Up out of nothing enters the
// first, which is the row a list with no current row moves to.
void ChoiceScene::nudge(int8_t direction) {
  if (items_.empty()) return;
  if (nullable_ && hasValue_ && direction < 0 && value_ == items_.front().key) {
    value_ = String();
    hasValue_ = false;
    rebuild();
    preview();
    return;
  }
  if (!hasValue_) {
    if (direction > 0) {
      value_ = items_.front().key;
      hasValue_ = true;
      rebuild();
    }
    preview();
    return;
  }
  if (list_ && list_->nudge(direction)) value_ = list_->selected();
  preview();
}

void ChoiceScene::rebuild() {
  pitch_ = choicePitch();
  list_ = std::make_unique<RowList>(pitch_, rowsOf(items_), heldKey(), true);
}

// The key the list is seated on: nothing while the setting holds nothing, the held key when an
// entry names it, and the first entry's key when none does, which is what makes a key no entry
// names show the first.
String ChoiceScene::heldKey() const {
  if (!hasValue_ || items_.empty()) return String();
  for (const ChoiceItem& item : items_) {
    if (item.key == value_) return value_;
  }
  return items_.front().key;
}

void ChoiceScene::preview() {
  if (forward_ && hasValue_) forward_(value_);
}

String ChoiceScene::text() const {
  const int16_t at = index();
  if (!hasValue_ || at < 0 || at >= static_cast<int16_t>(items_.size())) return "--";
  return items_[at].title;
}

std::any ChoiceScene::value() const { return hasValue_ ? std::any(value_) : std::any(); }

void ChoiceScene::canceled() {
  value_ = opened_;
  hasValue_ = hasOpened_;
  rebuild();
}

int16_t ChoiceScene::index() const { return list_ ? list_->current() : -1; }

// The entries drawn by the list into the value band, centred down it and scrolled to keep the entry
// showing in view; a band with nothing showing prints the row a first entry would take.
gfx::PaintNodeRef ChoiceScene::pageContents(gfx::Size) {
  if (pitch_ != choicePitch()) rebuild();
  if (!hasValue_ || items_.empty() || !list_) {
    return gfx::callable([](gfx::Canvas& canvas) {
      const int16_t pitch = choicePitch();
      const int16_t top =
          gfx::alignedOffset(static_cast<int16_t>(canvas.height() - pitch), gfx::Justify::Center);
      gfx::View cell = canvas.child(gfx::Rect(0, top, canvas.width(), pitch));
      gfx::text("--")->draw(cell);
    });
  }
  return gfx::callable([this](gfx::Canvas& canvas) { list_->draw(canvas, gfx::Justify::Center); });
}

}  // namespace gardener