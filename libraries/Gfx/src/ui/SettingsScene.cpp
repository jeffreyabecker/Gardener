#include "ui/SettingsScene.h"

#include <string.h>

#include "ui/Drawing.h"

namespace gardener::gfx::ui {
namespace {

// One row of the built-in face, which is the pitch the rows and the heading are drawn at.
constexpr int16_t Row = 8;

// The row as the list prints it: the setting's label, an equals sign and the value it holds now.
Paint rowText(const SettingsRow& setting, const std::any& value) {
  const String printed = setting.label() + "=" + setting.text(value);
  return [printed](Canvas& row) {
    text(Rect(0, 0, row.width(), Row), printed.c_str(), nullptr, 1)(row);
  };
}

}  // namespace

SettingsScene::SettingsScene(GestureQueue& gestures, Ux& ux, const String& title)
    : Scene(gestures), title_(title), ux_(ux) {}

void SettingsScene::set(const std::vector<SettingsRow*>& rows) {
  rows_ = rows;
  values_.clear();
  for (const SettingsRow* row : rows_) {
    if (row != nullptr) values_[row->key()] = row->currentValue();
  }
  opened_ = values_;
  if (selected_ >= static_cast<int16_t>(rows_.size())) selected_ = 0;
}

bool SettingsScene::handle(const char* gesture) {
  if (strcmp(gesture, "up:single") == 0) {
    move(-1);
    return true;
  }
  if (strcmp(gesture, "down:single") == 0) {
    move(1);
    return true;
  }
  if (strcmp(gesture, "forward:single") == 0) {
    open();
    return true;
  }
  if (strcmp(gesture, "ok:single") == 0) {
    ux_.pop();
    return true;
  }
  if (strcmp(gesture, "back:single") == 0 || strcmp(gesture, "cancel:single") == 0) {
    values_ = opened_;
    ux_.pop();
    return true;
  }
  return false;
}

void SettingsScene::move(int8_t direction) {
  const int16_t count = static_cast<int16_t>(rows_.size());
  if (count == 0) return;
  selected_ = static_cast<int16_t>((selected_ + direction + count) % count);
}

// Forward opens the row's editor with the value the row holds now, and what that editor hands back
// is written under the row's key. A scene that was cancelled hands back the value it opened on, so
// the write is the same either way.
void SettingsScene::open() {
  if (selected_ < 0 || selected_ >= static_cast<int16_t>(rows_.size())) return;
  SettingsRow* row = rows_[selected_];
  if (row == nullptr) return;
  const auto held = values_.find(row->key());
  const std::any value = held == values_.end() ? std::any() : held->second;
  Scene* editor = row->scene(value);
  if (editor == nullptr) return;
  ux_.push(*editor, [this, row](Scene& popped) { values_[row->key()] = popped.value(); });
}

// The heading, when the list has one, and the rows under it one built-in row each: the window
// scrolled to keep the row the keys are on in view, that row inverted across the width, and the
// scrollbar down the right of the rows when they do not all fit.
void SettingsScene::drawPage(Canvas& canvas) {
  const int16_t heading = title_.isEmpty() ? 0 : Row;
  if (heading > 0) {
    text(Rect(0, 0, canvas.width(), Row), title_.c_str(), nullptr, 1)(canvas);
  }
  if (rows_.empty()) return;
  Canvas band = canvas.child(Rect(0, heading, canvas.width(), canvas.height() - heading));
  const int16_t count = static_cast<int16_t>(rows_.size());
  int16_t visible = static_cast<int16_t>(band.height() / Row);
  if (visible < 1) visible = 1;
  if (visible > count) visible = count;
  const int16_t first = selected_ >= visible ? static_cast<int16_t>(selected_ - visible + 1) : 0;
  const bool scrolls = count > visible;
  const int16_t rowsWidth =
      scrolls ? static_cast<int16_t>(band.width() - ScrollbarColumn) : band.width();
  for (int16_t line = 0; line < visible; line++) {
    const int16_t at = static_cast<int16_t>(first + line);
    SettingsRow* row = rows_[at];
    if (row == nullptr) continue;
    const auto held = values_.find(row->key());
    const std::any value = held == values_.end() ? std::any() : held->second;
    Canvas cell = band.child(Rect(0, static_cast<int16_t>(line * Row), rowsWidth, Row));
    rowText(*row, value)(cell);
    if (at == selected_) cell.invert(Rect(0, 0, cell.width(), Row));
  }
  if (scrolls) drawScrollbar(band, first, count, visible);
}

}  // namespace gardener::gfx::ui
