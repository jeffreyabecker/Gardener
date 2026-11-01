#include "panel/page/EditorScene.h"

#include <stdio.h>

#include <utility>

#include "layout/Drawing.h"
#include "layout/Layout.h"
#include "panel/page/SettingsScene.h"
#include "shapes/Shapes.h"
#include "shapes/Text.h"

namespace gardener {
namespace {

// The settings view's geometry, from view.be: the paper outside the border, the border, the paper
// inside it, a chrome row of the Adafruit face, and the gap between a chrome row and the body.
constexpr int16_t SettingsMargin = 1;
constexpr int16_t SettingsBorder = 1;
constexpr int16_t SettingsPadding = 1;
constexpr int16_t SettingsRowHeight = 8;
constexpr int16_t SettingsGap = 2;
constexpr int16_t SettingsChrome = 2 * (SettingsRowHeight + SettingsGap);

// The three bands of an editor page, as shares of the page it is given: the title, the contents
// and the hint. The two chrome bands are the same height, so the contents take what they leave.
constexpr float TitleBandPct = 0.15f;
constexpr float ContentsBandPct = 0.7f;
constexpr float HintBandPct = 0.15f;

// One line of chrome centred in the band it is given: the family's cut that the band's height
// holds, placed by the leaf itself, so the title is the size its band can carry.
gfx::PaintNodeRef centredLine(const char* text, const gfx::Family* family) {
  return gfx::text(gfx::Box::whole(), text, family);
}

// The arrows a hint band marks its keys with: as wide and as tall as the ink of the glyphs they
// stand in for, and the paper that separates one item of the band from the next.
constexpr int16_t ArrowWidth = 5;
constexpr int16_t ArrowHeight = 7;
constexpr int16_t HintGap = 2;

// The ink a run of arrows takes: side by side with the gap between them, one arrow tall.
gfx::Size arrowsInk(uint8_t count) {
  const int16_t width = static_cast<int16_t>(count * ArrowWidth + (count - 1) * HintGap);
  return {width, ArrowHeight};
}

// One item of the band: the ink it draws and the paper that separates it from the next item.
gfx::Child hintItem(gfx::PaintNodeRef paint, gfx::Size ink) {
  return gfx::Child{gfx::Cell(static_cast<float>(ink.width + HintGap), 1.0f),
                    gfx::aligned(gfx::sized(std::move(paint), ink), gfx::Justify::Start)};
}

}  // namespace

EditorScene::EditorScene(GestureQueue& gestures, Ux& ux, String& label)
    : Scene(gestures), ux_(ux), label_(label) {}

void EditorScene::setValidator(CurriedValidator validator) { validator_ = std::move(validator); }

gfx::Child EditorScene::arrowsHint(HintArrow first, HintArrow second) {
  const uint8_t count = second == HintArrow::None ? 1 : 2;
  return hintItem(gfx::callable([first, second](gfx::Canvas& cell) {
                    // The box one arrow fills in the item's ink: the first at its left, the second after
                    // the gap between them.
                    const auto draw = [&cell](HintArrow arrow, int16_t left) {
                      const gfx::Rect at(left, 0, ArrowWidth, ArrowHeight);
                      switch (arrow) {
                        case HintArrow::Up:
                          cell.draw(gfx::Triangle::up(at, gfx::Ink, true));
                          break;
                        case HintArrow::Down:
                          cell.draw(gfx::Triangle::down(at, gfx::Ink, true));
                          break;
                        case HintArrow::Back:
                          cell.draw(gfx::Triangle::left(at, gfx::Ink, true));
                          break;
                        case HintArrow::Forward:
                          cell.draw(gfx::Triangle::right(at, gfx::Ink, true));
                          break;
                        case HintArrow::None:
                          break;
                      }
                    };
                    draw(first, 0);
                    if (second != HintArrow::None)
                      draw(second, static_cast<int16_t>(ArrowWidth + HintGap));
                  }),
                  arrowsInk(count));
}

gfx::Child EditorScene::wordsHint(const char* text) {
  return hintItem(gfx::text(gfx::Box::whole(), text, rowFace()),
                  gfx::measureText(rowFace(), text, 0, SettingsRowHeight));
}

gfx::Children EditorScene::hintItems() {
  return gfx::children(arrowsHint(HintArrow::Up, HintArrow::Down), wordsHint(hint()));
}

bool EditorScene::handle(const char* gesture) {
  if (closed_) return false;
  if (Gestures::is(gesture, Gestures::UpSingle)) {
    refusal_ = String();
    nudge(1);
    return true;
  }
  if (Gestures::is(gesture, Gestures::DownSingle)) {
    refusal_ = String();
    nudge(-1);
    return true;
  }
  if (Gestures::is(gesture, Gestures::OkSingle)) {
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
  if (Gestures::is(gesture, Gestures::CancelSingle)) {
    closed_ = true;
    canceled();
    ux_.pop();
    return true;
  }
  return false;
}

void EditorScene::drawPage(gfx::Canvas& canvas) { pageComponents()->draw(canvas); }

gfx::PaintNodeRef EditorScene::pageComponents() {
  return gfx::column(1.0f,
                     gfx::children(gfx::Child{gfx::Cell(1.0f, TitleBandPct), titleComponent()},
                                   gfx::Child{gfx::Cell(1.0f, ContentsBandPct), pageContents()},
                                   gfx::Child{gfx::Cell(1.0f, HintBandPct), hintComponent()}));
}

gfx::PaintNodeRef EditorScene::pageContents() { return gfx::PaintNodeRef(); }
gfx::PaintNodeRef EditorScene::titleComponent() {
  // The composition's design, which the title band's height spends on the largest cut it holds.
  return centredLine(title(), nullptr);
}

gfx::PaintNodeRef EditorScene::hintComponent() {
  // A refused value answers in words, and the band is those words: the marks would say the keys
  // step a value the editor has not taken.
  if (!refusal_.isEmpty()) return centredLine(refusal_.c_str(), rowFace());
  gfx::Children items = hintItems();
  // The gap separates an item from the next, so nothing follows the last of them.
  if (!items.empty())
    items.back().cell.width = static_cast<float>(items.back().cell.width - HintGap);
  return gfx::row(1.0f, std::move(items),
                  gfx::Justification{gfx::Justify::Center, gfx::Justify::Center});
}

}  // namespace gardener