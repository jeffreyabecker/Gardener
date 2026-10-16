#include "hal/impl/AdafruitDisplay.h"

#include <type_traits>

namespace gardener {
namespace {

// Nothing else spells the verb list out, so a verb added to Display and not implemented here
// would leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<AdafruitDisplay>::value,
              "AdafruitDisplay must implement every Display verb");

}  // namespace

void AdafruitDisplay::text(int16_t x, int16_t y, const String& value, uint8_t size,
                           bool inverted) {
  draw(x, y, value.c_str(), size, inverted);
}

void AdafruitDisplay::text(int16_t x, int16_t y, const char* value, uint8_t size,
                           bool inverted) {
  draw(x, y, value, size, inverted);
}

void AdafruitDisplay::textBounds(const String& value, uint8_t size, int16_t& outWidth,
                                 int16_t& outHeight) {
  measure(value.c_str(), size, outWidth, outHeight);
}

void AdafruitDisplay::textBounds(const char* value, uint8_t size, int16_t& outWidth,
                                 int16_t& outHeight) {
  measure(value, size, outWidth, outHeight);
}

void AdafruitDisplay::draw(int16_t x, int16_t y, const char* value, uint8_t size,
                           bool inverted) {
  panel_->setTextSize(size);
  // An opaque background is what makes inverted text readable; Adafruit_GFX spells "no
  // background" by setting the ink and the paper to the same value.
  if (inverted) {
    panel_->setTextColor(kPaper, kInk);
  } else {
    panel_->setTextColor(kInk);
  }
  panel_->setCursor(x, y);
  panel_->print(value);
}

void AdafruitDisplay::measure(const char* value, uint8_t size, int16_t& outWidth,
                              int16_t& outHeight) {
  // Adafruit_GFX measures at the size the cursor is set to, so the size has to be set first.
  panel_->setTextSize(size);
  int16_t boxX = 0;
  int16_t boxY = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  panel_->getTextBounds(value, 0, 0, &boxX, &boxY, &width, &height);
  outWidth = (int16_t)width;
  outHeight = (int16_t)height;
}

}  // namespace gardener
