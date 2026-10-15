#pragma once

#include <Arduino.h>
#include <Adafruit_SH110X.h>
#include <Wire.h>

#include <stdint.h>

#include "hal/Pins.h"

namespace eggplan {

// SH1106 panel access: drawing primitives and a flush, nothing else. Page layout,
// headers, bars and the battery icon live in ui/Renderer, so this class has no opinion
// about what the screen looks like.
class Display {
 public:
  explicit Display(TwoWire& wire) : gfx_(pins::OLED_WIDTH, pins::OLED_HEIGHT, &wire, -1) {}

  bool begin();
  bool present() const { return present_; }
  int16_t width() const { return gfx_.width(); }

  void clear();
  void flush();

  void text(int16_t x, int16_t y, const char* text, uint8_t size = 1);
  void textInverted(int16_t x, int16_t y, const char* text, uint8_t size = 1);
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled = false);

 private:
  Adafruit_SH1106G gfx_;
  bool present_ = false;
};

}  // namespace eggplan
