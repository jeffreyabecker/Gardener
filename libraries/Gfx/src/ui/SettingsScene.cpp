#include "ui/SettingsScene.h"

namespace gardener::gfx::ui {

SettingsScene::SettingsScene(GestureQueue& gestures, const String& title)
    : Scene(gestures), title_(title) {}

void SettingsScene::set(const std::vector<SettingsRow*>& rows) {
  rows_ = rows;
  values_.clear();
  for (const SettingsRow* row : rows_) {
    if (row != nullptr) values_[row->key()] = row->currentValue();
  }
  if (selected_ >= static_cast<int16_t>(rows_.size())) selected_ = 0;
}

bool SettingsScene::handle(const char* gesture) {
  (void)gesture;
  return false;
}

void SettingsScene::drawPage(Canvas& canvas) { (void)canvas; }

}  // namespace gardener::gfx::ui
