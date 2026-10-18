#include "hal/impl/AdafruitDisplay.h"

#include <Adafruit_SH110X.h>
#include <Wire.h>
#include <type_traits>

#include "core/Config.h"

namespace gardener {
namespace {

// Nothing else spells the verb list out, so a verb added to Display and not implemented here
// would leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<AdafruitDisplay>::value,
              "AdafruitDisplay must implement every Display verb");

}  // namespace

bool AdafruitDisplay::begin() {
  // The panel is the first part on the bus and the one with no fallback, so it brings the bus up;
  // the sensor comes up after it.
  Wire.begin(cfg::I2C_SDA, cfg::I2C_SCL, cfg::I2C_FREQ_HZ);

  // The driver is new'd and never deleted: it lives for the life of the device, whose destructor
  // never runs, so nothing owns it.
  Adafruit_SH1106G* driver = new Adafruit_SH1106G(panel::WIDTH, panel::HEIGHT, &Wire, -1);

  // The STEMMA QT breakout has no reset line, so the library must not poke one.
  if (!driver->begin(panel::I2C_ADDR, false)) {
    delete driver;
    return false;
  }
  panel_ = driver;
  return true;
}

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
