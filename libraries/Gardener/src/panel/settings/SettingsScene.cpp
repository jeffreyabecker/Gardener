#include "panel/settings/SettingsScene.h"

#include "Bitmap.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener {
namespace {

// One row of the face the settings rows are drawn in, which is the pitch the rows and the heading
// are drawn at: a ttyp0 cut's shortest line box is ten rows, so the panel's design cannot be drawn
// in one.
constexpr int16_t Row = 8;

// The row as the list prints it: the setting's label, an equals sign and the value it holds now.
gfx::PaintNodeRef rowText(const SettingsRow& setting, const std::any& value, gfx::Box box) {
  const String printed = setting.label() + "=" + setting.text(value);
  return gfx::text(box, printed, nullptr);
}

// The row as the list draws it: the mark the row answers, in the columns its own header names and
// centred down the row, then the label and value in what is left. A row with no mark — or one whose
// string names no bitmap — is the text alone, starting at the left of the row as it always has.
gfx::PaintNodeRef rowContent(const SettingsRow& setting, const std::any& value) {
  const String& icon = setting.icon();
  const gfx::Size mark = gfx::bitmapSize(icon.c_str());
  if (mark.width <= 0 || mark.height <= 0) {
    return rowText(setting, value, gfx::Box(0, 0, 0, Row, true, false));
  }
  const int16_t top = static_cast<int16_t>((Row - mark.height) / 2);
  return gfx::row(1.0f,
                  gfx::children(gfx::Child{gfx::Cell(static_cast<float>(mark.width), 1.0f),
                                           gfx::icon(gfx::Box(0, top, mark.width, mark.height),
                                                     icon.c_str())},
                                gfx::Child{gfx::Cell(gfx::Cell::Whole, 1.0f),
                                           rowText(setting, value, gfx::Box::whole())}));
}

}  // namespace

CurriedValidator SettingsRow::check(const std::vector<SettingsRow*>& settings,
                                    const SettingValues& values) const {
  if (!validator_) return nullptr;
  return [this, &settings, &values](const std::any& proposed) {
    return validator_(settings, values, proposed);
  };
}

// The empty answer a kind that prints no mark gives: one String for every such row, so asking costs
// a reference rather than a copy.
const String& SettingsRow::icon() const {
  static const String none;
  return none;
}

// A kind that steps no number — a bool, a choice — has no range for a section to move.
void SettingsRow::limit(float, float) {}

SettingsScene::SettingsScene(GestureQueue& gestures, Ux& ux, const String& title,
                             const std::vector<SettingsRow*>& rows, const SettingValues& values)
    : Scene(gestures), title_(title), ux_(ux), rows_(rows) {
  for (size_t i = 0; i < rows_.size(); i++) {
    const SettingsRow* row = rows_[i];
    if (row == nullptr) continue;
    const auto held = values.find(row->key());
    values_[row->key()] = held == values.end() ? row->defaultValue() : held->second;
    if (row->hidden()) continue;
    visible_.push_back(static_cast<int16_t>(i));
  }
  opened_ = values_;
}

bool SettingsScene::handle(const char* gesture) {
  if (Gestures::is(gesture, Gestures::UpSingle)) {
    move(-1);
    return true;
  }
  if (Gestures::is(gesture, Gestures::DownSingle)) {
    move(1);
    return true;
  }
  if (Gestures::is(gesture, Gestures::ForwardSingle)) {
    open();
    return true;
  }
  if (Gestures::is(gesture, Gestures::OkSingle)) {
    ux_.pop();
    return true;
  }
  if (Gestures::isAny(gesture, {Gestures::BackSingle, Gestures::CancelSingle})) {
    values_ = opened_;
    ux_.pop();
    return true;
  }
  return false;
}

void SettingsScene::move(int8_t direction) {
  const int16_t count = static_cast<int16_t>(visible_.size());
  if (count == 0) return;
  selected_ = static_cast<int16_t>((selected_ + direction + count) % count);
}

// Forward opens the row's editor with the value the list holds for the row's key, and what that
// editor hands back is written under it. A scene that was cancelled hands back the value it opened
// on, and one whose row's check refused a value stays open rather than hand it back, so the write
// is always a value the row's declaration accepts.
void SettingsScene::open() {
  if (selected_ < 0 || selected_ >= static_cast<int16_t>(visible_.size())) return;
  SettingsRow* row = rows_[visible_[selected_]];
  if (row == nullptr) return;
  const auto held = values_.find(row->key());
  const std::any value = held == values_.end() ? std::any() : held->second;
  std::unique_ptr<EditorScene> editor = row->scene(value);
  if (!editor) return;
  editor->setValidator(row->check(rows_, values_));
  ux_.push(std::move(editor), [this, row](Scene& popped) { values_[row->key()] = popped.value(); });
}

// The heading, when the list has one, and the rows under it one Adafruit row each: the window
// scrolled to keep the row the keys are on in view, that row inverted across the width, and the
// scrollbar down the right of the rows when they do not all fit.
void SettingsScene::drawPage(gfx::Canvas& canvas) {
  const int16_t heading = title_.isEmpty() ? 0 : Row;
  if (heading > 0) {
    gfx::text(gfx::Box(0, 0, 0, Row, true, false), title_, nullptr)->draw(canvas);
  }
  if (visible_.empty()) return;
  gfx::View band = canvas.child(gfx::Rect(0, heading, canvas.width(), canvas.height() - heading));
  const int16_t count = static_cast<int16_t>(visible_.size());
  int16_t visible = static_cast<int16_t>(band.height() / Row);
  if (visible < 1) visible = 1;
  if (visible > count) visible = count;
  const int16_t first = selected_ >= visible ? static_cast<int16_t>(selected_ - visible + 1) : 0;
  const bool scrolls = count > visible;
  const int16_t rowsWidth =
      scrolls ? static_cast<int16_t>(band.width() - gfx::ScrollbarColumn) : band.width();
  for (int16_t line = 0; line < visible; line++) {
    const int16_t at = static_cast<int16_t>(first + line);
    SettingsRow* row = rows_[visible_[at]];
    if (row == nullptr) continue;
    const auto held = values_.find(row->key());
    const std::any value = held == values_.end() ? std::any() : held->second;
    gfx::View cell = band.child(gfx::Rect(0, static_cast<int16_t>(line * Row), rowsWidth, Row));
    gfx::PaintNodeRef content = rowContent(*row, value);
    if (at == selected_) content = gfx::inverted(std::move(content));
    content->draw(cell);
  }
  if (scrolls) gfx::drawScrollbar(band, first, count, visible);
}

}  // namespace gardener