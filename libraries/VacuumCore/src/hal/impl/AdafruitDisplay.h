#pragma once

#include <Adafruit_GrayOLED.h>
#include <stdint.h>

#include "hal/Display.h"

namespace gardener {

// The fitted panel: an Adafruit 1.3" 128x64 monochrome SH1106 on the STEMMA QT bus. Its geometry
// and address are the part's own, which is why they sit beside its driver rather than in the
// board's pin map — the 128x32 alternative is the same wiring with different numbers.
namespace panel {
constexpr uint16_t WIDTH = 128;
constexpr uint16_t HEIGHT = 64;
constexpr uint8_t I2C_ADDR = 0x3C;
}  // namespace panel

// The Display for a monochrome Adafruit GrayOLED panel, with the monochrome colours supplied here
// so nothing outside this file has to know that 1 lights a pixel.
//
// It takes the GrayOLED base rather than the fitted panel, because every verb it needs is on the
// base: the drawing from Adafruit_GFX, `display()` from Adafruit_GrayOLED.
//
// The driver is built up in begin(), once the I2C bus exists; nothing draws before it returns
// true.
class AdafruitDisplay : public Display {
 public:
  // The driver outlives this adapter and is never deleted: Adafruit_GrayOLED has no virtual
  // destructor, and the panel lives for the life of the device.
  ~AdafruitDisplay() override = default;

  // --- the panel -----------------------------------------------------------
  bool begin() override;
  void flush() override { panel_->display(); }

  // --- geometry ------------------------------------------------------------
  int16_t width() const override { return panel_->width(); }
  int16_t height() const override { return panel_->height(); }
  uint8_t rotation() const override { return panel_->getRotation(); }
  void setRotation(uint8_t rotation) override { panel_->setRotation(rotation); }

  // --- the screen ----------------------------------------------------------
  void clear() override { panel_->fillScreen(kPaper); }
  void invert(bool on) override {
    inverted_ = on;
    panel_->invertDisplay(on);
  }
  bool inverted() const override { return inverted_; }

  // --- pixels, lines and shapes --------------------------------------------
  void pixel(int16_t x, int16_t y) override { panel_->drawPixel(x, y, kInk); }
  void clearRect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    panel_->fillRect(x, y, w, h, kPaper);
  }
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) override {
    panel_->drawLine(x0, y0, x1, y1, kInk);
  }
  void hLine(int16_t x, int16_t y, int16_t w) override { panel_->drawFastHLine(x, y, w, kInk); }
  void vLine(int16_t x, int16_t y, int16_t h) override { panel_->drawFastVLine(x, y, h, kInk); }
  void rect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    panel_->drawRect(x, y, w, h, kInk);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    panel_->fillRect(x, y, w, h, kInk);
  }
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) override {
    panel_->drawRoundRect(x, y, w, h, r, kInk);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) override {
    panel_->fillRoundRect(x, y, w, h, r, kInk);
  }
  void circle(int16_t x, int16_t y, int16_t r) override { panel_->drawCircle(x, y, r, kInk); }
  void fillCircle(int16_t x, int16_t y, int16_t r) override { panel_->fillCircle(x, y, r, kInk); }
  void ellipse(int16_t x, int16_t y, int16_t rw, int16_t rh) override {
    panel_->drawEllipse(x, y, rw, rh, kInk);
  }
  void fillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh) override {
    panel_->fillEllipse(x, y, rw, rh, kInk);
  }
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                int16_t y2) override {
    panel_->drawTriangle(x0, y0, x1, y1, x2, y2, kInk);
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                    int16_t y2) override {
    panel_->fillTriangle(x0, y0, x1, y1, x2, y2, kInk);
  }
  void rotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle) override {
    panel_->drawRotatedRect(cx, cy, w, h, angle, kInk);
  }
  void fillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h,
                       int16_t angle) override {
    panel_->fillRotatedRect(cx, cy, w, h, angle, kInk);
  }

  // --- images --------------------------------------------------------------
  void bitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h) override {
    panel_->drawBitmap(x, y, bitmap, w, h, kInk);
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

  // The driver's own record of the inversion, because the library keeps the bit where it draws
  // rather than somewhere it can be read back.
  bool inverted_ = false;

  void draw(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted);
  void measure(const char* value, uint8_t size, int16_t& outWidth, int16_t& outHeight);

  Adafruit_GrayOLED* panel_ = nullptr;
};

}  // namespace gardener
