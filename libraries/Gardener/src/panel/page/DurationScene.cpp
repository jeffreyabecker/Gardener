#include "panel/page/DurationScene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Text.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener::panel::page {
namespace {

// The face the two fields are drawn at.
constexpr uint8_t FieldScale = 2;

// A band holding one string: the dashes centred at the field face.
gfx::PaintNodeRef centredDashes() {
  const gfx::Size ink = gfx::shapes::measureText(nullptr, "--", FieldScale);
  return gfx::aligned(
      gfx::sized(gfx::borrowedText(gfx::Box::whole(), "--", nullptr, FieldScale), ink));
}

// One field of the pair: its title centred in an equal share of the row, and that title inverted
// when it is the field the keys are stepping. The alignment places the title's own ink inside the
// share, so the inverted box is the field rather than the whole cell.
gfx::Child field(const char* title, bool showing) {
  const gfx::Size ink = gfx::shapes::measureText(nullptr, title, FieldScale);
  gfx::PaintNodeRef content =
      gfx::borrowedText(gfx::Box::whole(), title, nullptr, FieldScale);
  if (showing) content = gfx::inverted(std::move(content));
  return gfx::Child{gfx::Cell(0.5f, 1.0f), gfx::aligned(gfx::sized(std::move(content), ink))};
}

}  // namespace

DurationScene::DurationScene(GestureQueue& gestures, Ux& ux, String& label, float current,
                             float step, float min, float max, bool nullable)
    : EditorScene(gestures, ux, label),
      current_(current),
      opened_(current),
      step_(step < 1 ? 1 : static_cast<int32_t>(step)),
      min_(min),
      max_(max),
      nullable_(nullable) {}

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

std::any DurationScene::value() const {
  return hasValue() ? std::any(whole()) : std::any();
}

const char* DurationScene::hint() {
  return hintOr(stage_ == Stage::Minutes ? "\x1E/\x1F MIN  \x1A=NEXT"
                                         : "\x1E/\x1F SEC  OK=SAVE");
}

bool DurationScene::handle(const char* gesture) {
  if (strcmp(gesture, "forward:single") == 0) {
    stage_ = Stage::Seconds;
    return true;
  }
  if (strcmp(gesture, "back:single") == 0) {
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
// inverted. A setting holding nothing has no fields to show, so the band prints `--`.
gfx::PaintNodeRef DurationScene::pageContents() {
  if (!hasValue()) {
    return centredDashes();
  }
  minutes_ = minutesText();
  seconds_ = secondsText();
  const bool minutes = stage_ == Stage::Minutes;
  return gfx::row(1.0f, gfx::children(field(minutes_.c_str(), minutes),
                                     field(seconds_.c_str(), !minutes)));
}

}  // namespace gardener::panel::page
