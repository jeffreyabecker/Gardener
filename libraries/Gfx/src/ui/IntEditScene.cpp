#include "ui/IntEditScene.h"

#include <stdio.h>
#include <string.h>

#include "gfx/Text.h"
#include "ui/Drawing.h"

namespace gardener::gfx::ui {
namespace {

// The rows every value editor's column shares: a label row and a hint row at the built-in face,
// with the paper and the body between them.
constexpr int16_t EditorTextRow = 8;
constexpr int16_t EditorPaperRow = 2;

// The face the value is drawn at, and the bar under it.
constexpr uint8_t ValueScale = 2;
constexpr int16_t BarHeight = 6;
constexpr const char* Hint = "\x1E/\x1F  OK=SAVE";

// A string centred across the canvas it is drawn on, `y` rows down from its top.
void drawCentred(Canvas& canvas, int16_t y, const char* value, uint8_t scale) {
  const shapes::TextBox ink = shapes::measureText(nullptr, value, scale);
  const int16_t x = static_cast<int16_t>((canvas.width() - ink.width) / 2);
  text(Rect(x, y, ink.width, ink.height), value, nullptr, scale)(canvas);
}

}  // namespace

IntEditScene::IntEditScene(GestureQueue& gestures, const char* label, int32_t value, bool hasValue,
                           int32_t step, Bounds bounds, bool nullable)
    : Scene(gestures),
      label_(label == nullptr ? "" : label),
      value_(value),
      hasValue_(hasValue),
      opened_(value),
      hasOpened_(hasValue),
      step_(step < 1 ? 1 : step),
      bounds_(bounds),
      nullable_(nullable) {}

void IntEditScene::nudge(int8_t direction) {
  const int32_t low = bounds_.hasMin ? bounds_.min : 0;
  if (nullable_) {
    if (!hasValue_) {
      if (direction < 0) return;
      value_ = low;
      hasValue_ = true;
      return;
    }
    if (direction < 0 && value_ == low) {
      hasValue_ = false;
      return;
    }
  }
  value_ += direction * step_;
  if (bounds_.hasMin && value_ < bounds_.min) value_ = bounds_.min;
  if (bounds_.hasMax && value_ > bounds_.max) value_ = bounds_.max;
}

bool IntEditScene::handle(const char* gesture) {
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
    return true;
  }
  if (strcmp(gesture, "cancel:single") == 0) {
    value_ = opened_;
    hasValue_ = hasOpened_;
    closed_ = true;
    return true;
  }
  return false;
}

void IntEditScene::drawPage(Canvas& canvas) {
  char printed[16];
  if (hasValue_) {
    snprintf(printed, sizeof(printed), "%ld", static_cast<long>(value_));
  } else {
    snprintf(printed, sizeof(printed), "--");
  }

  const shapes::TextBox ink = shapes::measureText(nullptr, printed, ValueScale);
  int16_t filled = 0;
  if (hasValue_ && bounds_.hasMax) {
    const int32_t low = bounds_.hasMin ? bounds_.min : 0;
    if (bounds_.max > low) {
      filled = static_cast<int16_t>((value_ - low) * 100 / (bounds_.max - low));
    }
  }

  const int16_t bodyY = EditorTextRow + EditorPaperRow;
  drawCentred(canvas, 0, label_, 1);
  drawCentred(canvas, bodyY, printed, ValueScale);
  if (bounds_.hasMin && bounds_.hasMax) {
    progressBar(Rect(0, static_cast<int16_t>(bodyY + ink.height + EditorPaperRow),
                     canvas.width(), BarHeight),
                filled)(canvas);
  }
  drawCentred(canvas, static_cast<int16_t>(canvas.height() - EditorTextRow), Hint, 1);
}

}  // namespace gardener::gfx::ui
