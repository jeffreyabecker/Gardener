#include "hal/Display.h"

#include "core/Log.h"

namespace eggplan {

bool Display::begin() {
  // The STEMMA QT breakout has no reset line, so do not let the library poke one.
  present_ = gfx_.begin(address_, false);
  if (!present_) {
    LOGE("oled: not found at 0x%02X", address_);
    return false;
  }
  clear();
  flush();
  LOGI("oled: SH1106 online at 0x%02X", address_);
  return true;
}

void Display::clear() { gfx_.clearDisplay(); }

void Display::flush() { gfx_.display(); }

void Display::text(int16_t x, int16_t y, const char* text, uint8_t size) {
  gfx_.setTextColor(SH110X_WHITE);
  gfx_.setTextSize(size);
  gfx_.setCursor(x, y);
  gfx_.print(text ? text : "");
}

void Display::textInverted(int16_t x, int16_t y, const char* text, uint8_t size) {
  gfx_.setTextColor(SH110X_BLACK, SH110X_WHITE);
  gfx_.setTextSize(size);
  gfx_.setCursor(x, y);
  gfx_.print(text ? text : "");
  gfx_.setTextColor(SH110X_WHITE);
}

void Display::rect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled) {
  if (filled) {
    gfx_.fillRect(x, y, w, h, SH110X_WHITE);
  } else {
    gfx_.drawRect(x, y, w, h, SH110X_WHITE);
  }
}

void Display::hline(int16_t x, int16_t y, int16_t w) {
  gfx_.drawFastHLine(x, y, w, SH110X_WHITE);
}

void Display::line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  gfx_.drawLine(x0, y0, x1, y1, SH110X_WHITE);
}

}  // namespace eggplan
