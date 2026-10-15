#pragma once

#include <Arduino.h>
#include <Adafruit_SH110X.h>
#include <Wire.h>

#include <stdint.h>

namespace eggplan {

// SH1106 panel access: drawing primitives and a flush, nothing else. Page layout,
// headers, bars and the graph live in ui/Renderer, so this class has no opinion
// about what the screen looks like.
class Display {
 public:
  Display(TwoWire& wire, uint8_t address, uint16_t width, uint16_t height)
      : gfx_(width, height, &wire, -1), address_(address) {}

  bool begin();
  bool present() const { return present_; }
  int16_t width() const { return gfx_.width(); }
  int16_t height() const { return gfx_.height(); }

  void clear();
  void flush();

  void text(int16_t x, int16_t y, const char* text, uint8_t size = 1);
  void textInverted(int16_t x, int16_t y, const char* text, uint8_t size = 1);
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled = false);
  void hline(int16_t x, int16_t y, int16_t w);
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

 private:
  Adafruit_SH1106G gfx_;
  uint8_t address_;
  bool present_ = false;
};

}  // namespace eggplan
