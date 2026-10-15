#include "hal/Display.h"

namespace eggplan {

bool Display::begin() {
  // The STEMMA QT breakout has no reset line, so do not let the library poke one.
  present_ = gfx_.begin(pins::OLED_I2C_ADDR, false);
  if (!present_) return false;
  clear();
  flush();
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

}  // namespace eggplan
