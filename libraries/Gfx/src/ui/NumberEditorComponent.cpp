#include "ui/NumberEditorComponent.h"

#include <math.h>
#include <stdio.h>

#include "gfx/Shapes.h"
#include "gfx/Text.h"

namespace gardener::gfx::ui {
namespace {

// The face the number is drawn at, the bar under it and the paper between the two.
constexpr uint8_t ValueScale = 2;
constexpr int16_t BarHeight = 6;
constexpr int16_t BarGap = 2;

// The band: the number at the value face and, when the setting has both bounds, the bar under it,
// the two centred in the canvas the paint is run on.
Paint numberBand(String printed, bool bar, int16_t percent) {
  return [printed, bar, percent](Canvas& canvas) {
    const shapes::TextBox ink = shapes::measureText(nullptr, printed.c_str(), ValueScale);
    const int16_t tall = static_cast<int16_t>(ink.height + (bar ? BarGap + BarHeight : 0));
    const int16_t top = static_cast<int16_t>((canvas.height() - tall) / 2);
    const int16_t left = static_cast<int16_t>((canvas.width() - ink.width) / 2);
    text(Rect(left, top, ink.width, ink.height), printed.c_str(), nullptr, ValueScale)(canvas);
    if (bar) {
      const int16_t barTop = static_cast<int16_t>(top + ink.height + BarGap);
      progressBar(Rect(0, barTop, canvas.width(), BarHeight), percent)(canvas);
    }
  };
}

}  // namespace

NumberEditorComponent::NumberEditorComponent(float current, float step, float min, float max,
                                             bool nullable)
    : current_(current), step_(step), min_(min), max_(max), nullable_(nullable) {}

bool NumberEditorComponent::hasValue() const { return !isnan(current_); }

String NumberEditorComponent::text() const {
  if (!hasValue()) return "--";
  char printed[16];
  snprintf(printed, sizeof(printed), "%g", static_cast<double>(current_));
  return String(printed);
}

void NumberEditorComponent::nudge(int8_t direction) {
  if (nullable_) {
    if (!hasValue()) {
      if (direction > 0) current_ = floor();
      return;
    }
    if (direction < 0 && current_ == floor()) {
      current_ = NAN;
      return;
    }
  }
  current_ = within(current_ + direction * step_);
}

float NumberEditorComponent::floor() const { return isfinite(min_) ? min_ : 0.0f; }

float NumberEditorComponent::within(float value) const {
  if (value < min_) value = min_;
  if (value > max_) value = max_;
  return value;
}

Paint NumberEditorComponent::draw() const {
  const String printed = text();
  const bool bar = isfinite(min_) && isfinite(max_) && max_ > min_;
  int16_t percent = 0;
  if (bar && hasValue()) {
    percent = static_cast<int16_t>((current_ - min_) * 100 / (max_ - min_));
  }
  return numberBand(printed, bar, percent);
}

}  // namespace gardener::gfx::ui
