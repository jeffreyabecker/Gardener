#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#include <functional>
#include <utility>

#include <Gfx.h>

#include "hal/Display.h"

namespace gardener {

// The Display for a Gfx panel device: the device owns the frame and the glass, and a Canvas over
// that frame supplies every drawing verb the app has. The device is a value member, constructed
// from the arguments handed to this constructor, so the composition root supplies the part's
// geometry and wiring and this file knows nothing about I2C, SPI or any other bus.
//
// The callback is the one thing that cannot happen at construction: a callback run at begin(), once
// the caller has brought the bus up, which lets the part come up and reports whether it answered.
template <typename Device>
class GfxDisplay : public Display {
 public:
  template <typename... Args>
  GfxDisplay(std::function<bool(Device&)> onBegin, Args&&... args)
      : onBegin_(onBegin),
        device_(std::forward<Args>(args)...),
        canvas_(device_.buffer(), device_.size(), device_.size(), layoutFor(device_)) {}

  ~GfxDisplay() override = default;

  bool begin() override { return onBegin_(device_); }
  void display() override { canvas_.flush(device_); }

  // The device behind the verbs, for a caller that has to reach it directly — the host harness
  // saves the frame this panel drew.
  Device& driver() { return device_; }

  int16_t width() const override { return canvas_.width(); }
  int16_t height() const override { return canvas_.height(); }
  int16_t pageWidth() const override { return canvas_.width(); }
  int16_t pageHeight() const override { return canvas_.height(); }
  uint8_t rotation() const override { return rotation_; }
  void setRotation(uint8_t rotation) override {
    rotation_ = static_cast<uint8_t>(rotation & 3);
    canvas_.setRotation(static_cast<int16_t>(rotation_ * 90));
  }

  void clear(uint8_t color) override {
    canvas_.draw(gfx::shapes::FillRect(0, 0, canvas_.width(), canvas_.height(), displayColor(color)));
  }
  void setInverted(bool on) override {
    inverted_ = on;
    device_.invertDisplay(on);
  }
  bool inverted() const override { return inverted_; }

  void pixel(int16_t x, int16_t y, uint8_t color) override {
    canvas_.setPixel(x, y, displayColor(color));
  }
  void invert(int16_t x, int16_t y, int16_t w, int16_t h) override {
    canvas_.invert(gfx::Rect(x, y, w, h));
  }
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) override {
    canvas_.draw(gfx::shapes::Line(x0, y0, x1, y1, displayColor(color)));
  }
  void hLine(int16_t x, int16_t y, int16_t w, uint8_t color) override {
    canvas_.draw(gfx::shapes::HLine(x, y, w, displayColor(color)));
  }
  void vLine(int16_t x, int16_t y, int16_t h, uint8_t color) override {
    canvas_.draw(gfx::shapes::VLine(x, y, h, displayColor(color)));
  }
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) override {
    canvas_.draw(gfx::shapes::Rect(x, y, w, h, displayColor(color)));
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) override {
    canvas_.draw(gfx::shapes::FillRect(x, y, w, h, displayColor(color)));
  }
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color) override {
    canvas_.draw(gfx::shapes::RoundRect(x, y, w, h, r, displayColor(color)));
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint8_t color) override {
    canvas_.draw(gfx::shapes::FillRoundRect(x, y, w, h, r, displayColor(color)));
  }
  void circle(int16_t x, int16_t y, int16_t r, uint8_t color) override {
    canvas_.draw(gfx::shapes::Circle(x, y, r, displayColor(color)));
  }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint8_t color) override {
    canvas_.draw(gfx::shapes::FillCircle(x, y, r, displayColor(color)));
  }
  void ellipse(int16_t x, int16_t y, int16_t rw, int16_t rh, uint8_t color) override {
    canvas_.draw(gfx::shapes::Ellipse(x, y, rw, rh, displayColor(color)));
  }
  void fillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh, uint8_t color) override {
    canvas_.draw(gfx::shapes::FillEllipse(x, y, rw, rh, displayColor(color)));
  }
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                uint8_t color) override {
    canvas_.draw(gfx::shapes::Triangle(x0, y0, x1, y1, x2, y2, displayColor(color)));
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint8_t color) override {
    canvas_.draw(gfx::shapes::FillTriangle(x0, y0, x1, y1, x2, y2, displayColor(color)));
  }
  void rotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle,
                   uint8_t color) override {
    canvas_.draw(gfx::shapes::RotatedRect(cx, cy, w, h, angle, displayColor(color)));
  }
  void fillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle,
                       uint8_t color) override {
    canvas_.draw(gfx::shapes::FillRotatedRect(cx, cy, w, h, angle, displayColor(color)));
  }
  void bitmap(int16_t x, int16_t y, const uint8_t* bits) override {
    canvas_.draw(gfx::shapes::MonoBitmap(x, y, bits, displayColor(Ink)));
  }

  void setFont(const gfx::Font* font) override { face_ = font; }
  void text(int16_t x, int16_t y, const String& value, uint8_t size, bool inverted) override {
    text(x, y, value.c_str(), size, inverted);
  }
  void text(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) override {
    if (value == nullptr) return;
    if (face_ == nullptr) {
      // The built-in face is drawn from the top of a five-by-seven cell; Gfx Text draws from the
      // baseline seven rows below it.
      const int16_t baseline = static_cast<int16_t>(y + 7 * size);
      if (inverted) {
        const int16_t width = static_cast<int16_t>(6 * size * strlen(value));
        const int16_t height = static_cast<int16_t>(8 * size);
        canvas_.draw(gfx::shapes::FillRect(x, y, width, height, displayColor(Ink)));
        canvas_.draw(gfx::shapes::Text(x, baseline, value, nullptr, size, displayColor(Paper)));
      } else {
        canvas_.draw(gfx::shapes::Text(x, baseline, value, nullptr, size, displayColor(Ink)));
      }
      return;
    }
    // A compiled face has no background, so inverted is the same as ink, as it is on the old panel.
    canvas_.draw(gfx::shapes::Text(x, y, value, face_, size, displayColor(Ink)));
  }
  void textBounds(const String& value, uint8_t size, int16_t& outWidth,
                  int16_t& outHeight) override {
    textBounds(value.c_str(), size, outWidth, outHeight);
  }
  void textBounds(const char* value, uint8_t size, int16_t& outWidth,
                  int16_t& outHeight) override {
    const gfx::shapes::TextBox box = gfx::shapes::measureText(face_, value, size);
    outWidth = box.width;
    outHeight = box.height;
  }
  void glyph(int16_t x, int16_t y, uint8_t code, uint8_t size) override {
    gfx::shapes::drawGlyph(canvas_, x, y, code, size, displayColor(Ink));
  }

 private:
  static gfx::PixelLayout layoutFor(const Device& device) {
    return device.bitsPerPixel() == 1 ? gfx::PixelLayout::Packed1 : gfx::PixelLayout::Gray8;
  }

  // The app's ink and paper are 1 and 0; a Packed1 or Gray8 frame wants 255 and 0.
  static uint8_t displayColor(uint8_t color) { return color == Paper ? 0 : 255; }

  std::function<bool(Device&)> onBegin_;
  Device device_;
  gfx::Canvas canvas_;
  const gfx::Font* face_ = nullptr;
  uint8_t rotation_ = 0;
  bool inverted_ = false;
};

}  // namespace gardener
