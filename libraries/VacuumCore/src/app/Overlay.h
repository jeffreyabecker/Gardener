#pragma once

#include <stdint.h>

#include "app/VacuumSystem.h"
#include "hal/Battery.h"
#include "hal/Display.h"

namespace gardener {

// The display the app draws through: it wraps any Display, and puts the device's own status on top
// of whatever was drawn there just before the frame goes to the glass.
//
// Everything else is passed straight through, so a page cannot tell the difference — which is
// the point. The battery, the run state's icon and the two ways a controller can reach the device
// have to be on the panel whatever page is showing, and nothing that draws a page should have to
// remember them.
class Overlay : public Display {
 public:
  Overlay(Display* inner, const Battery* battery, const VacuumSystem* vacuum)
      : inner_(inner), battery_(battery), vacuum_(vacuum) {}
  ~Overlay() override = default;

  // --- the panel -----------------------------------------------------------
  bool begin() override { return inner_->begin(); }
  void display() override;  // draws the overlay first, then sends the frame

  // Which ways in are up. The loop is the one that can see the serial link; Bluetooth is not fitted,
  // so it is false until a radio is.
  void setConnections(bool serial, bool bluetooth);

  // --- geometry ------------------------------------------------------------
  int16_t width() const override { return inner_->width(); }
  int16_t height() const override { return inner_->height(); }
  uint8_t rotation() const override { return inner_->rotation(); }
  void setRotation(uint8_t rotation) override { inner_->setRotation(rotation); }

  // --- the screen ----------------------------------------------------------
  void clear() override { inner_->clear(); }
  void invert(bool on) override { inner_->invert(on); }
  bool inverted() const override { return inner_->inverted(); }

  // --- pixels, lines and shapes --------------------------------------------
  void pixel(int16_t x, int16_t y) override { inner_->pixel(x, y); }
  void clearRect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    inner_->clearRect(x, y, w, h);
  }
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) override {
    inner_->line(x0, y0, x1, y1);
  }
  void hLine(int16_t x, int16_t y, int16_t w) override { inner_->hLine(x, y, w); }
  void vLine(int16_t x, int16_t y, int16_t h) override { inner_->vLine(x, y, h); }
  void rect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    inner_->rect(x, y, w, h);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h) override {
    inner_->fillRect(x, y, w, h);
  }
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) override {
    inner_->roundRect(x, y, w, h, r);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) override {
    inner_->fillRoundRect(x, y, w, h, r);
  }
  void circle(int16_t x, int16_t y, int16_t r) override { inner_->circle(x, y, r); }
  void fillCircle(int16_t x, int16_t y, int16_t r) override { inner_->fillCircle(x, y, r); }
  void ellipse(int16_t x, int16_t y, int16_t rw, int16_t rh) override {
    inner_->ellipse(x, y, rw, rh);
  }
  void fillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh) override {
    inner_->fillEllipse(x, y, rw, rh);
  }
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                int16_t y2) override {
    inner_->triangle(x0, y0, x1, y1, x2, y2);
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                    int16_t y2) override {
    inner_->fillTriangle(x0, y0, x1, y1, x2, y2);
  }
  void rotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle) override {
    inner_->rotatedRect(cx, cy, w, h, angle);
  }
  void fillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h,
                       int16_t angle) override {
    inner_->fillRotatedRect(cx, cy, w, h, angle);
  }

  // --- images --------------------------------------------------------------
  void bitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h) override {
    inner_->bitmap(x, y, bitmap, w, h);
  }

  // --- text ----------------------------------------------------------------
  void setFont(const GFXfont* font) override { inner_->setFont(font); }
  void text(int16_t x, int16_t y, const String& value, uint8_t size,
            bool inverted) override {
    inner_->text(x, y, value, size, inverted);
  }
  void text(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) override {
    inner_->text(x, y, value, size, inverted);
  }
  void textBounds(const String& value, uint8_t size, int16_t& outWidth,
                  int16_t& outHeight) override {
    inner_->textBounds(value, size, outWidth, outHeight);
  }
  void textBounds(const char* value, uint8_t size, int16_t& outWidth,
                  int16_t& outHeight) override {
    inner_->textBounds(value, size, outWidth, outHeight);
  }

 private:
  void drawStateIcon();
  void chevrons(int16_t x, int16_t y, bool down);
  void drawConnections();
  void serialIcon(int16_t x, int16_t y);
  void bluetoothIcon(int16_t x, int16_t y);
  void drawBattery();
  void batteryIcon(int16_t x, int16_t y, uint8_t percent);

  Display* inner_;
  const Battery* battery_;
  const VacuumSystem* vacuum_;
  bool serial_ = false;
  bool bluetooth_ = false;
};

}  // namespace gardener
