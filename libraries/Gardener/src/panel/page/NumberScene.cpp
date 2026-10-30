#include "panel/page/NumberScene.h"

#include <math.h>
#include <stdio.h>

#include <utility>

#include "Text.h"
#include "Drawing.h"
#include "Layout.h"

namespace gardener::panel::page {
namespace {

// The face the number is drawn at, the bar under it and the paper between the two.
constexpr uint8_t ValueScale = 2;
constexpr int16_t BarHeight = 6;
constexpr int16_t BarGap = 2;

// The band, as the column the editor page draws it in: the number at the value face, two pixels of
// paper and, when the setting declares both bounds, the bar that shows where the number sits
// between them. The column is centred in the band and the number centred in the column, so the
// band reads as one thing rather than as a number with a bar under it. `printed` is the scene's
// own string, which outlives the paint.
gfx::Paint numberBand(const char* printed, gfx::shapes::TextBox ink, bool bar, int16_t percent) {
  const gfx::Child number{gfx::Cell(static_cast<float>(ink.width),
                                    static_cast<float>(ink.height)),
                          gfx::text(gfx::Rect(0, 0, ink.width, ink.height), printed, nullptr,
                                    ValueScale)};
  gfx::Justification centred;
  centred.along = gfx::Justify::Center;
  centred.across = gfx::Justify::Center;
  if (!bar) return gfx::column(1.0f, {number}, centred);
  const gfx::Child gap{gfx::Cell(1.0f, BarGap), gfx::Paint()};
  const gfx::Child filled{gfx::Cell(1.0f, BarHeight), [percent](gfx::Canvas& own) {
    gfx::progressBar(gfx::Rect(0, 0, own.width(), own.height()), percent)(own);
  }};
  return gfx::column(1.0f, {number, gap, filled}, centred);
}

}  // namespace

NumberScene::NumberScene(GestureQueue& gestures, Ux& ux, String& label, float current, float step,
                         float min, float max, bool nullable, NumberFormatter format)
    : EditorScene(gestures, ux, label),
      number_(current),
      opened_(current),
      step_(step),
      min_(min),
      max_(max),
      nullable_(nullable),
      format_(std::move(format)) {}

bool NumberScene::hasValue() const { return !isnan(number_); }

String NumberScene::text() const {
  if (!hasValue()) return "--";
  if (format_) return format_(number_);
  char printed[16];
  snprintf(printed, sizeof(printed), "%g", static_cast<double>(number_));
  return String(printed);
}

std::any NumberScene::value() const { return hasValue() ? std::any(number_) : std::any(); }

void NumberScene::nudge(int8_t direction) {
  if (nullable_) {
    if (!hasValue()) {
      if (direction > 0) number_ = floor();
      return;
    }
    if (direction < 0 && number_ == floor()) {
      number_ = NAN;
      return;
    }
  }
  number_ = within(number_ + direction * step_);
}

void NumberScene::canceled() { number_ = opened_; }

float NumberScene::floor() const { return isfinite(min_) ? min_ : 0.0f; }

float NumberScene::within(float value) const {
  if (value < min_) value = min_;
  if (value > max_) value = max_;
  return value;
}

gfx::Paint NumberScene::pageContents() {
  printed_ = text();
  const gfx::shapes::TextBox ink = gfx::shapes::measureText(nullptr, printed_.c_str(), ValueScale);
  const bool bar = isfinite(min_) && isfinite(max_) && max_ > min_;
  int16_t percent = 0;
  if (bar && hasValue()) {
    percent = static_cast<int16_t>((number_ - min_) * 100 / (max_ - min_));
  }
  return numberBand(printed_.c_str(), ink, bar, percent);
}

}  // namespace gardener::panel::page
