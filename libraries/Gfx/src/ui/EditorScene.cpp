#include "ui/EditorScene.h"

#include <stdio.h>
#include <string.h>

#include "gfx/Shapes.h"
#include "gfx/Text.h"
#include "ui/Drawing.h"
#include "ui/Layout.h"

namespace gardener::gfx::ui {
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

// A string centred across and down the canvas it is drawn on, at the built-in face.
void drawCentred(Canvas& canvas, const char* value) {
  const shapes::TextBox ink = shapes::measureText(nullptr, value, 1);
  const int16_t x = static_cast<int16_t>((canvas.width() - ink.width) / 2);
  const int16_t y = static_cast<int16_t>((canvas.height() - ink.height) / 2);
  text(Rect(x, y, ink.width, ink.height), value, nullptr, 1)(canvas);
}

}  // namespace

EditorScene::EditorScene(GestureQueue& gestures, String& label)
    : Scene(gestures), label_(label) {}



bool EditorScene::handle(const char* gesture) {
  if (closed_) return false;
  if (strcmp(gesture, "up:single") == 0) {
    nudge(1);
    return true;
  }
  if (strcmp(gesture, "down:single") == 0) {
    nudge(-1);
    return true;
  }
  if (strcmp(gesture, "ok:single") == 0) {
    closed_ = true;
    accepted();
    return true;
  }
  if (strcmp(gesture, "cancel:single") == 0) {
    value_ = opened_;
    hasValue_ = hasOpened_;
    closed_ = true;
    canceled();
    return true;
  }
  return false;
}

void EditorScene::drawPage(Canvas& canvas) {
   pageComponents()(canvas);
}
  Paint EditorScene::pageComponents() {
    return column(1.0f, {
        Child{Cell(1.0f, TitleBandPct), [this](Canvas& canvas) { drawCentred(canvas, title()); }},
        Child{Cell(1.0f, ContentsBandPct), pageContents()},
        Child{Cell(1.0f, HintBandPct), [this](Canvas& canvas) { drawCentred(canvas, hint()); }},
    });
  }
  Paint EditorScene::pageContents(){
    return Paint();
  }
}  // namespace gardener::gfx::ui
