#pragma once

#include <functional>
#include <stdint.h>

#include "hal/Display.h"

namespace gardener {

// The Display for an Adafruit GrayOLED driver: it supplies the monochrome ink and paper and
// forwards every other verb, so nothing that draws can tell this from the driver behind it.
//
// The driver is a value member, constructed from the arguments handed to this constructor, so the
// composition root supplies the part's geometry and wiring and this file knows nothing about I2C,
// SPI or any other bus. The first argument is the one thing that cannot happen at construction: a
// callback run at begin(), once the caller has brought the bus up, which lets the part come up and
// reports whether it answered.
//
// The driver is a template parameter because the fitted part is not this file's business.
template <typename Driver>
class AdafruitDisplay : public Display {
 public:
  // `onBegin` runs at begin() with the constructed driver; everything else goes to its constructor.
  template <typename... Args>
  AdafruitDisplay(std::function<bool(Driver&)> onBegin, Args... args)
      : onBegin_(onBegin), panel_(args...) {}

  ~AdafruitDisplay() override = default;

  // --- the panel -----------------------------------------------------------
  bool begin() override { return onBegin_(panel_); }
  void display() override { panel_.display(); }

  // --- geometry ------------------------------------------------------------
  int16_t width() const override { return panel_.width(); }
  int16_t height() const override { return panel_.height(); }
  uint8_t rotation() const override { return panel_.getRotation(); }
  void setRotation(uint8_t rotation) override { panel_.setRotation(rotation); }

  // --- the screen ----------------------------------------------------------
  void clear() override { panel_.fillScreen(kPaper); }
  void invert(bool on) override {
    inverted_ = on;
    panel_.invertDisplay(on);
  }
  bool inverted() const override { return inverted_; }

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
  void setFont(const GFXfont* font) override { panel_.setFont(font); }
  void text(int16_t x, int16_t y, const String& value, uint8_t size, bool inverted) override {
    draw(x, y, value.c_str(), size, inverted);
  }
  void text(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) override {
    draw(x, y, value, size, inverted);
  }
  void textBounds(const String& value, uint8_t size, int16_t& outWidth,
                  int16_t& outHeight) override {
    measure(value.c_str(), size, outWidth, outHeight);
  }
  void textBounds(const char* value, uint8_t size, int16_t& outWidth,
                  int16_t& outHeight) override {
    measure(value, size, outWidth, outHeight);
  }

 private:
  static constexpr uint16_t kInk = 1;
  static constexpr uint16_t kPaper = 0;

  void draw(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) {
    panel_.setTextSize(size);
    // An opaque background is what makes inverted text readable; Adafruit_GFX spells "no
    // background" by setting the ink and the paper to the same value.
    if (inverted) {
      panel_.setTextColor(kPaper, kInk);
    } else {
      panel_.setTextColor(kInk);
    }
    panel_.setCursor(x, y);
    panel_.print(value);
  }

  void measure(const char* value, uint8_t size, int16_t& outWidth, int16_t& outHeight) {
    // Adafruit_GFX measures at the size the cursor is set to, so the size has to be set first.
    panel_.setTextSize(size);
    int16_t boxX = 0;
    int16_t boxY = 0;
    uint16_t boxWidth = 0;
    uint16_t boxHeight = 0;
    panel_.getTextBounds(value, 0, 0, &boxX, &boxY, &boxWidth, &boxHeight);
    outWidth = (int16_t)boxWidth;
    outHeight = (int16_t)boxHeight;
  }

  // Run at begin(), once the caller's bus is up; the driver itself is constructed with the args
  // above, before any of that.
  std::function<bool(Driver&)> onBegin_;
  Driver panel_;
  // The driver's own record of the inversion, because the library keeps the bit where it draws
  // rather than somewhere it can be read back.
  bool inverted_ = false;
};

}  // namespace gardener