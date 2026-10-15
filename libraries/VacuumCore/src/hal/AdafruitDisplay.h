#pragma once

#include <Adafruit_SH110X.h>
#include <stdint.h>

#include "hal/Display.h"
#include "hal/Pins.h"

namespace gardener {

// The Display for this board: an Adafruit SH1106G panel, with the monochrome colours supplied
// here so nothing outside this file has to know that 1 lights a pixel.
//
// It names the concrete panel because bring-up is the one thing that is not shared: in this
// library every drawing verb comes from Adafruit_GFX, `display()` from Adafruit_GrayOLED, and
// `begin()` from the panel class itself. A different panel is therefore a different adapter - or
// a change to this one line - which is the price of a `begin()` that can actually probe.
class AdafruitDisplay : public Display {
 public:
  explicit AdafruitDisplay(Adafruit_SH1106G& panel) : panel_(panel) {}

  // --- the panel -----------------------------------------------------------
  bool begin() override;
  void flush() override { panel_.display(); }

  // --- geometry ------------------------------------------------------------
  int16_t width() const override { return panel_.width(); }
  int16_t height() const override { return panel_.height(); }
  uint8_t rotation() const override { return panel_.getRotation(); }
  void setRotation(uint8_t rotation) override { panel_.setRotation(rotation); }

  // --- the screen ----------------------------------------------------------
  void clear() override { panel_.fillScreen(kPaper); }
  void invert(bool on) override { panel_.invertDisplay(on); }

  // --- pixels, lines and shapes --------------------------------------------
  void pixel(int16_t x, int16_t y) override { panel_.drawPixel(x, y, kInk); }
  void clearRect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    panel_.fillRect(x, y, w, h, kPaper);
  }
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) override {
    panel_.drawLine(x0, y0, x1, y1, kInk);
  }
  void hLine(int16_t x, int16_t y, int16_t w) override { panel_.drawFastHLine(x, y, w, kInk); }
  void vLine(int16_t x, int16_t y, int16_t h) override { panel_.drawFastVLine(x, y, h, kInk); }
  void rect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    panel_.drawRect(x, y, w, h, kInk);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    panel_.fillRect(x, y, w, h, kInk);
  }
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) override {
    panel_.drawRoundRect(x, y, w, h, r, kInk);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) override {
    panel_.fillRoundRect(x, y, w, h, r, kInk);
  }
  void circle(int16_t x, int16_t y, int16_t r) override { panel_.drawCircle(x, y, r, kInk); }
  void fillCircle(int16_t x, int16_t y, int16_t r) override { panel_.fillCircle(x, y, r, kInk); }
  void ellipse(int16_t x, int16_t y, int16_t rw, int16_t rh) override {
    panel_.drawEllipse(x, y, rw, rh, kInk);
  }
  void fillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh) override {
    panel_.fillEllipse(x, y, rw, rh, kInk);
  }
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                int16_t y2) override {
    panel_.drawTriangle(x0, y0, x1, y1, x2, y2, kInk);
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                    int16_t y2) override {
    panel_.fillTriangle(x0, y0, x1, y1, x2, y2, kInk);
  }
  void rotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle) override {
    panel_.drawRotatedRect(cx, cy, w, h, angle, kInk);
  }
  void fillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h,
                       int16_t angle) override {
    panel_.fillRotatedRect(cx, cy, w, h, angle, kInk);
  }

  // --- images --------------------------------------------------------------
  void bitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h) override {
    panel_.drawBitmap(x, y, bitmap, w, h, kInk);
  }

  // --- text ----------------------------------------------------------------
  void text(int16_t x, int16_t y, const String& value, uint8_t size, bool inverted) override;
  void text(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) override;
  void textBounds(const String& value, uint8_t size, int16_t& outWidth,
                  int16_t& outHeight) override;
  void textBounds(const char* value, uint8_t size, int16_t& outWidth,
                  int16_t& outHeight) override;

 private:
  static constexpr uint16_t kInk = 1;
  static constexpr uint16_t kPaper = 0;

  void draw(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted);
  void measure(const char* value, uint8_t size, int16_t& outWidth, int16_t& outHeight);

  Adafruit_SH1106G& panel_;
};

}  // namespace gardener
