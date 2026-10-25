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

  // The driver behind the verbs, for a caller that has to reach it directly — the host harness
  // saves the frame this panel drew.
  Driver& driver() { return panel_; }

  // --- geometry ------------------------------------------------------------
  int16_t width() const override { return panel_.width(); }
  int16_t height() const override { return panel_.height(); }
  int16_t pageWidth() const override { return panel_.width(); }
  int16_t pageHeight() const override { return panel_.height(); }
  uint8_t rotation() const override { return panel_.getRotation(); }
  void setRotation(uint8_t rotation) override { panel_.setRotation(rotation); }

  // --- the screen ----------------------------------------------------------
  void clear(uint8_t color) override { panel_.fillScreen(color); }
  void setInverted(bool on) override {
    inverted_ = on;
    panel_.invertDisplay(on);
  }
  bool inverted() const override { return inverted_; }

  // --- pixels, lines and shapes --------------------------------------------
  void pixel(int16_t x, int16_t y, uint8_t color) override {
    panel_.drawPixel(x, y, color);
  }
  void invert(int16_t x, int16_t y, int16_t w, int16_t h) override {
    // Flip is Adafruit's own "toggle this pixel" colour: the monochrome drivers xor the bit
    // rather than write a value, so each pixel keeps its state until it is flipped.
    for (int16_t row = y; row < y + h; row++) {
      for (int16_t col = x; col < x + w; col++) {
        panel_.drawPixel(col, row, Flip);
      }
    }
  }
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) override {
    panel_.drawLine(x0, y0, x1, y1, color);
  }
  void hLine(int16_t x, int16_t y, int16_t w, uint8_t color) override {
    panel_.drawFastHLine(x, y, w, color);
  }
  void vLine(int16_t x, int16_t y, int16_t h, uint8_t color) override {
    panel_.drawFastVLine(x, y, h, color);
  }
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) override {
    panel_.drawRect(x, y, w, h, color);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) override {
    panel_.fillRect(x, y, w, h, color);
  }
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                 uint8_t color) override {
    panel_.drawRoundRect(x, y, w, h, r, color);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint8_t color) override {
    panel_.fillRoundRect(x, y, w, h, r, color);
  }
  void circle(int16_t x, int16_t y, int16_t r, uint8_t color) override {
    panel_.drawCircle(x, y, r, color);
  }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint8_t color) override {
    panel_.fillCircle(x, y, r, color);
  }
  void ellipse(int16_t x, int16_t y, int16_t rw, int16_t rh, uint8_t color) override {
    panel_.drawEllipse(x, y, rw, rh, color);
  }
  void fillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh, uint8_t color) override {
    panel_.fillEllipse(x, y, rw, rh, color);
  }
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                uint8_t color) override {
    panel_.drawTriangle(x0, y0, x1, y1, x2, y2, color);
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint8_t color) override {
    panel_.fillTriangle(x0, y0, x1, y1, x2, y2, color);
  }
  void rotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle,
                   uint8_t color) override {
    panel_.drawRotatedRect(cx, cy, w, h, angle, color);
  }
  void fillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle,
                       uint8_t color) override {
    panel_.fillRotatedRect(cx, cy, w, h, angle, color);
  }
  void bitmap(int16_t x, int16_t y, const uint8_t* bits) override {
    const int16_t width = bits[0];
    const int16_t height = bits[1];
    const int16_t stride = (width + 7) / 8;
    const uint8_t* rows = bits + 2;
    // The proposed hardware keeps the frame in a buffer, so one write per ink pixel is fast
    // enough and there is nothing to gain from coalescing runs.
    for (int16_t row = 0; row < height; row++) {
      const uint8_t* line = rows + row * stride;
      for (int16_t col = 0; col < width; col++) {
        if (inkAt(line, col)) hLine(x + col, y + row, 1, Ink);
      }
    }
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
  void glyph(int16_t x, int16_t y, uint8_t code, uint8_t size) override {
    // drawChar is the one verb that takes a raw code: print() would read 0x0A as a newline, and
    // the band's state icons need the glyph itself. Ink for both colours means no background.
    panel_.drawChar(x, y, code, Ink, Ink, size);
  }

 private:
  static constexpr uint16_t Flip = 2;

  void draw(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) {
    panel_.setTextSize(size);
    // An opaque background is what makes inverted text readable; Adafruit_GFX spells "no
    // background" by setting the ink and the paper to the same value.
    if (inverted) {
      panel_.setTextColor(Paper, Ink);
    } else {
      panel_.setTextColor(Ink);
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

  // The bit at one column of a packed row, most-significant bit first.
  static bool inkAt(const uint8_t* line, int16_t col) {
    return (line[col / 8] >> (7 - (col & 7))) & 1;
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