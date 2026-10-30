#include "panel/page/NumberScene.h"

#include <math.h>
#include <stdio.h>

#include <utility>

#include "Text.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener::panel::page {
namespace {

// The face the number is drawn at.
constexpr uint8_t ValueScale = 2;

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

// The number centred in the contents band at the value face.
gfx::PaintNodeRef NumberScene::pageContents() {
  printed_ = text();
  const gfx::Size ink = gfx::shapes::measureText(nullptr, printed_.c_str(), ValueScale);
  return gfx::aligned(gfx::sized(
      gfx::borrowedText(gfx::Box::whole(), printed_.c_str(), nullptr, ValueScale), ink));
}

}  // namespace gardener::panel::page
