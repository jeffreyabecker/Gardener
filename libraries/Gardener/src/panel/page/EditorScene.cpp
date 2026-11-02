#include "panel/page/EditorScene.h"

#include <stdio.h>

#include <utility>

#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener {
namespace {

// The three bands of an editor page, as shares of the page it is given: the title, the contents
// and the hint. The two chrome bands are the same height, so the contents take what they leave.
constexpr float TitleBandPct = 0.15f;
constexpr float ContentsBandPct = 0.7f;
constexpr float HintBandPct = 0.15f;

// The contents band of a page this size, as the layout divides it: the title's share off the top,
// and the contents taking their share of the height clamped to what the title left. It is the rule
// LayoutNode::spanOf applies, so a kind told this band is given the one its paint lands in.
gfx::Size contentsBand(gfx::Size page) {
  const int16_t title = gfx::resolveSpan(page.height, TitleBandPct);
  const int16_t room = static_cast<int16_t>(page.height - title);
  const int16_t contents = gfx::resolveSpan(page.height, ContentsBandPct);
  return {page.width, contents > room ? room : contents};
}

// One line of chrome centred in the band it is given: the family's cut that the band's height
// holds, placed by the leaf itself, so the title is the size its band can carry.
gfx::PaintNodeRef centredLine(const char* text, const gfx::Family* family) {
  return gfx::text(text, family);
}

// The face a hint band's marks draw in: the Adafruit face, whose line box is the eight rows a band
// gives it, so a mark is a glyph of that face rather than a shape sized to stand in for one.
const gfx::Family* hintFace() { return gfx::findFamily("adafruit"); }

// The band's one row: a step mark in a sixth of the width each, and the kind's words in the two
// thirds beside them.
constexpr float HintMarkShare = 1.0f / 6.0f;
constexpr float HintWordsShare = 2.0f / 3.0f;

}  // namespace

EditorScene::EditorScene(GestureQueue& gestures, Ux& ux, String& label)
    : Scene(gestures), ux_(ux), label_(label) {}

void EditorScene::setValidator(CurriedValidator validator) { validator_ = std::move(validator); }

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

void EditorScene::drawPage(gfx::Canvas& canvas) {
  pageComponents({canvas.width(), canvas.height()})->draw(canvas);
}

gfx::PaintNodeRef EditorScene::pageComponents(gfx::Size page) {
  return gfx::column(1.0f, gfx::row(TitleBandPct, titleComponent()),
                     gfx::row(ContentsBandPct, pageContents(contentsBand(page))),
                     gfx::row(HintBandPct, hintComponent()));
}

gfx::PaintNodeRef EditorScene::pageContents(gfx::Size) { return gfx::PaintNodeRef(); }
gfx::PaintNodeRef EditorScene::titleComponent() {
  // The composition's design, which the title band's height spends on the largest cut it holds.
  return centredLine(title(), nullptr);
}

gfx::PaintNodeRef EditorScene::hintComponent() {
  // A refused value answers in words, and the band is those words: the marks would say the keys
  // step a value the editor has not taken.
  if (!refusal_.isEmpty()) return centredLine(refusal_.c_str(), nullptr);
  return gfx::row(1.0f, gfx::column(HintMarkShare, gfx::text("\x1F", hintFace())),
                  gfx::column(HintMarkShare, gfx::text("\x1E", hintFace())),
                  gfx::column(HintWordsShare, gfx::text(hintText())));
}

}  // namespace gardener