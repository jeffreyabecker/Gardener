#include "panel/settings/SettingsScene.h"

#include <utility>

#include "layout/Drawing.h"

namespace gardener {
namespace {

// One row of the face the settings rows are drawn in, which is the pitch the rows and the heading
// are drawn at: a ttyp0 cut's shortest line box is ten rows, so the panel's design cannot be drawn
// in one.
constexpr int16_t Row = 8;

}  // namespace

CurriedValidator SettingConfig::check(const std::vector<SettingConfig*>& settings,
                                      const SettingValues& values) const {
  if (!validator_) return nullptr;
  return [this, &settings, &values](const std::any& proposed) {
    return validator_(settings, values, proposed);
  };
}

// The empty answer a kind that prints no mark gives: one String for every such row, so asking costs
// a reference rather than a copy.
const String& SettingConfig::icon() const {
  static const String none;
  return none;
}

SettingsScene::SettingsScene(GestureQueue& gestures, Ux& ux, const String& title,
                             const std::vector<SettingConfig*>& rows, const SettingValues& values)
    : Scene(gestures), title_(title), ux_(ux), settingsCfgs_(rows) {
  for (size_t i = 0; i < settingsCfgs_.size(); i++) {
    const SettingConfig* row = settingsCfgs_[i];
    if (row == nullptr) continue;
    const auto held = values.find(row->key());
    settingsValues_[row->key()] = held == values.end() ? row->defaultValue() : held->second;
    if (row->hidden()) continue;
    visible_.push_back(static_cast<int16_t>(i));
  }
  opened_ = settingsValues_;
  rebuild(visible_.empty() ? String() : settingsCfgs_[visible_.front()]->key());
}

// The list the keys move in, built from the values in force: one row per visible setting, its key
// as the value the keys select it by and the text the row prints for the value it holds now. A
// rebuild is a few Strings and their paints, and it is what an editor handing a value back costs.
void SettingsScene::rebuild(const String& selected) {
  std::vector<gfx::RowList::Row> rows;
  rows.reserve(visible_.size());
  for (int16_t at : visible_) {
    SettingConfig* row = settingsCfgs_[at];
    const auto held = settingsValues_.find(row->key());
    const std::any value = held == settingsValues_.end() ? std::any() : held->second;
    rows.push_back(gfx::RowList::textRow(row->key(), row->label(), row->icon()));
  }
  displayListItems_ = std::make_unique<gfx::RowList>(Row, std::move(rows), selected, true);
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
    settingsValues_ = opened_;
    ux_.pop();
    return true;
  }
  return false;
}

void SettingsScene::move(int8_t direction) {
  if (displayListItems_) displayListItems_->nudge(direction);
}

// Forward opens the row's editor with the value the list holds for the row's key, and what that
// editor hands back is written under it. A scene that was cancelled hands back the value it opened
// on, and one whose row's check refused a value stays open rather than hand it back, so the write
// is always a value the row's declaration accepts.
void SettingsScene::open() {
  const int16_t at = selected();
  if (at < 0 || at >= static_cast<int16_t>(visible_.size())) return;
  SettingConfig* row = settingsCfgs_[visible_[at]];
  if (row == nullptr) return;
  const auto held = settingsValues_.find(row->key());
  const std::any value = held == settingsValues_.end() ? std::any() : held->second;
  std::unique_ptr<EditorScene> editor = row->scene(value);
  if (!editor) return;
  editor->setValidator(row->check(settingsCfgs_, settingsValues_));
  // The edited row is the one the keys are on, so the rebuilt list selects its key and the row the
  // value was written to keeps the highlight.
  ux_.push(std::move(editor), [this, row](Scene& popped) {
    settingsValues_[row->key()] = popped.value();
    rebuild(row->key());
  });
}

// The heading, when the list has one, and the list's window under it: the rows scrolled to keep the
// row the keys are on in view, that row inverted across its cell, and the scrollbar down the right
// when they do not all fit.
void SettingsScene::drawPage(gfx::Canvas& canvas) {
  const int16_t heading = title_.isEmpty() ? 0 : Row;
  if (heading > 0) {
    gfx::text(gfx::Box(0, 0, 0, Row, true, false), title_, nullptr)->draw(canvas);
  }
  if (!displayListItems_) return;
  gfx::View band = canvas.child(gfx::Rect(0, heading, canvas.width(), canvas.height() - heading));
  displayListItems_->draw(band);
}

}  // namespace gardener