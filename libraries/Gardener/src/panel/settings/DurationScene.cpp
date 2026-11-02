#include "panel/settings/DurationScene.h"

#include <math.h>
#include <stdio.h>

#include "layout/Drawing.h"
#include "layout/Layout.h"
#include "shapes/Text.h"

namespace gardener {
namespace {

// One field of the pair: its title centred in an equal share of the row, and the whole share
// inverted when it is the field the keys are stepping.
gfx::Child field(const char* title, bool showing) {
  gfx::PaintNodeRef content = gfx::text(title);
  if (showing) content = gfx::inverted(std::move(content));
  return gfx::column(0.5f, std::move(content));
}

// The minutes band's cells: a mark in a sixth of the width each, and the words in the third that is
// left.
constexpr float MarkShare = 1.0f / 6.0f;
constexpr float WordsShare = 1.0f / 3.0f;

}  // namespace

DurationScene::DurationScene(GestureQueue& gestures, Ux& ux, String& label, float current,
                             float step, float min, float max, bool nullable)
    : EditorScene(gestures, ux, label), current_(current), opened_(current),
      step_(step < 1 ? 1 : static_cast<int32_t>(step)), min_(min), max_(max), nullable_(nullable) {}

bool DurationScene::hasValue() const { return !isnan(current_); }

int32_t DurationScene::whole() const { return static_cast<int32_t>(current_); }

int32_t DurationScene::floor() const {
  if (!isfinite(min_)) return 0;
  const int32_t low = static_cast<int32_t>(min_);
  return low > 0 ? low : 0;
}

int32_t DurationScene::held(int32_t seconds) const {
  const int32_t low = floor();
  const int32_t high = isfinite(max_) ? static_cast<int32_t>(max_) : INT32_MAX;
  if (seconds < low) seconds = low;
  if (seconds > high) seconds = seconds > low ? high : low;
  return seconds;
}

String DurationScene::text() const {
  if (!hasValue()) return "--";
  char printed[16];
  gfx::durationText(gfx::DurationFormat::Minutes, whole(), true, printed, sizeof(printed));
  return String(printed);
}

String DurationScene::minutesText() const {
  char printed[12];
  snprintf(printed, sizeof(printed), "%ldm", static_cast<long>(whole() / 60));
  return String(printed);
}

String DurationScene::secondsText() const {
  char printed[12];
  snprintf(printed, sizeof(printed), "%lds", static_cast<long>(whole() % 60));
  return String(printed);
}

std::any DurationScene::value() const { return hasValue() ? std::any(whole()) : std::any(); }

// The minutes stage's words name the field the keys step and say that Forward moves to the seconds;
// the seconds stage has nowhere left to go, so OK is what it offers.
const char* DurationScene::hintText() {
  return stage_ == Stage::Minutes ? "MIN" : "SEC OK=SAVE";
}

// The minutes stage says which field the keys step and marks Forward as the way on to the seconds;
// the seconds stage has nowhere left to go, so it is the page's own band and OK is what it offers.
gfx::PaintNodeRef DurationScene::hintComponent() {
  // A refused value is the page's answer in words, whatever stage the editor is in.
  if (!refusal().isEmpty() || stage_ == Stage::Seconds) return EditorScene::hintComponent();
  const gfx::Family* face = gfx::findFamily("adafruit");
  return gfx::row(1.0f, gfx::column(MarkShare, gfx::text("\x1F", face)),
                  gfx::column(MarkShare, gfx::text("\x1E", face)),
                  gfx::column(MarkShare, gfx::text(hintText())),
                  gfx::column(MarkShare, gfx::text("\x10", face)),
                  gfx::column(WordsShare, gfx::text("NEXT")));
}

bool DurationScene::handle(const char* gesture) {
  if (Gestures::is(gesture, Gestures::ForwardSingle)) {
    stage_ = Stage::Seconds;
    return true;
  }
  if (Gestures::is(gesture, Gestures::BackSingle)) {
    stage_ = Stage::Minutes;
    return true;
  }
  return EditorScene::handle(gesture);
}

void DurationScene::nudge(int8_t direction) {
  if (nullable_) {
    const int32_t low = held(0);
    if (!hasValue()) {
      if (direction > 0) current_ = static_cast<float>(low);
      return;
    }
    if (direction < 0 && whole() == low) {
      current_ = NAN;
      return;
    }
  }
  const int32_t amount = stage_ == Stage::Minutes ? 60 : step_;
  current_ = static_cast<float>(held(whole() + direction * amount));
}

void DurationScene::canceled() { current_ = opened_; }

// The two fields side by side, each an equal share of the band, with the one the keys are stepping
// inverted. A setting holding nothing has no fields to show, so the band prints `--` where they
// would be, at the cut the band's height takes.
gfx::PaintNodeRef DurationScene::pageContents(gfx::Size) {
  if (!hasValue()) {
    return gfx::text("--");
  }
  minutes_ = minutesText();
  seconds_ = secondsText();
  const bool minutes = stage_ == Stage::Minutes;
  return gfx::row(1.0f, field(minutes_.c_str(), minutes), field(seconds_.c_str(), !minutes));
}

}  // namespace gardener