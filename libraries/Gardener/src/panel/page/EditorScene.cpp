#include "panel/page/EditorScene.h"

#include <stdio.h>
#include <string.h>

#include <utility>

#include "Text.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener::panel::page {
namespace {

// The settings view's geometry, from view.be: the paper outside the border, the border, the paper
// inside it, a chrome row of the built-in face, and the gap between a chrome row and the body.
constexpr int16_t SettingsMargin = 1;
constexpr int16_t SettingsBorder = 1;
constexpr int16_t SettingsPadding = 1;
constexpr int16_t SettingsRow = 8;
constexpr int16_t SettingsGap = 2;
constexpr int16_t SettingsChrome = 2 * (SettingsRow + SettingsGap);

// The three bands of an editor page, as shares of the page it is given: the title, the contents
// and the hint. The two chrome bands are the same height, so the contents take what they leave.
constexpr float TitleBandPct = 0.15f;
constexpr float ContentsBandPct = 0.7f;
constexpr float HintBandPct = 0.15f;

// One line of chrome centred in the band it is given: the ink the string measures, placed by the
// layout's own alignment.
gfx::PaintNodeRef centredLine(const char* text) {
  const gfx::Size ink = gfx::shapes::measureText(nullptr, text, 1);
  return gfx::aligned(gfx::sized(gfx::borrowedText(gfx::Box::whole(), text), ink));
}

}  // namespace

EditorScene::EditorScene(GestureQueue& gestures, Ux& ux, String& label)
    : Scene(gestures), ux_(ux), label_(label) {}

void EditorScene::setValidator(CurriedValidator validator) { validator_ = std::move(validator); }

const char* EditorScene::hintOr(const char* text) const {
  return refusal_.isEmpty() ? text : refusal_.c_str();
}

bool EditorScene::handle(const char* gesture) {
  if (closed_) return false;
  if (strcmp(gesture, "up:single") == 0) {
    refusal_ = String();
    nudge(1);
    return true;
  }
  if (strcmp(gesture, "down:single") == 0) {
    refusal_ = String();
    nudge(-1);
    return true;
  }
  if (strcmp(gesture, "ok:single") == 0) {
    if (validator_) {
      const std::pair<bool, String> answer = validator_(value());
      if (!answer.first) {
        refusal_ = answer.second;
        return true;
      }
      refusal_ = String();
    }
    closed_ = true;
    accepted();
    ux_.pop();
    return true;
  }
  if (strcmp(gesture, "cancel:single") == 0) {
    closed_ = true;
    canceled();
    ux_.pop();
    return true;
  }
  return false;
}

void EditorScene::drawPage(gfx::Canvas& canvas) {
  pageComponents()->draw(canvas);
}

gfx::PaintNodeRef EditorScene::pageComponents() {
  return gfx::column(
      1.0f, gfx::children(gfx::Child{gfx::Cell(1.0f, TitleBandPct), centredLine(title())},
                          gfx::Child{gfx::Cell(1.0f, ContentsBandPct), pageContents()},
                          gfx::Child{gfx::Cell(1.0f, HintBandPct), centredLine(hint())}));
}

gfx::PaintNodeRef EditorScene::pageContents() {
  return gfx::PaintNodeRef();
}

}  // namespace gardener::panel::page
