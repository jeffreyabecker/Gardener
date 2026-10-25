#pragma once

#include <stdint.h>

#include "VacuumSystem.h"
#include "hal/Battery.h"
#include "hal/Display.h"

namespace gardener {

class Config;

// The display the app draws through: it wraps any Display, and puts the device's own status in a
// band across the bottom of whatever was drawn there just before the frame goes to the glass.
//
// Everything else is passed straight through, so a page cannot tell the difference — which is
// the point. The battery, the run state's icon, the unit the glass prints in and the two ways a
// controller can reach the device have to be on the panel whatever page is showing, and nothing
// that draws a page should have to remember them.
class Overlay : public Display {
 public:
  Overlay(Display* inner, const Battery* battery, const VacuumSystem* vacuum,
          const Config* config)
      : inner_(inner), battery_(battery), vacuum_(vacuum), config_(config) {}
  ~Overlay() override = default;

  // --- the panel -----------------------------------------------------------
  bool begin() override { return inner_->begin(); }
  void display() override;  // draws the overlay first, then sends the frame

  // Which ways in are up. The loop is the one that can see both links, and presence is all this
  // says: a connected link that has not acquired control still lights its icon.
  void setConnections(bool serial, bool bluetooth);

  // --- geometry ------------------------------------------------------------
  int16_t width() const override { return inner_->width(); }
  int16_t height() const override { return inner_->height(); }
  // The page area: the panel minus the status band across the bottom.
  int16_t pageWidth() const override;
  int16_t pageHeight() const override;
  uint8_t rotation() const override { return inner_->rotation(); }
  void setRotation(uint8_t rotation) override { inner_->setRotation(rotation); }

  // --- the screen ----------------------------------------------------------
  void clear(uint8_t color) override { inner_->clear(color); }
  void setInverted(bool on) override { inner_->setInverted(on); }
  bool inverted() const override { return inner_->inverted(); }

  // --- pixels, lines and shapes --------------------------------------------
  void pixel(int16_t x, int16_t y, uint8_t color) override { inner_->pixel(x, y, color); }
  void invert(int16_t x, int16_t y, int16_t w, int16_t h) override {
    inner_->invert(x, y, w, h);
  }
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) override {
    inner_->line(x0, y0, x1, y1, color);
  }
  void hLine(int16_t x, int16_t y, int16_t w, uint8_t color) override {
    inner_->hLine(x, y, w, color);
  }
  void vLine(int16_t x, int16_t y, int16_t h, uint8_t color) override {
    inner_->vLine(x, y, h, color);
  }
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) override {
    inner_->rect(x, y, w, h, color);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) override {
    inner_->fillRect(x, y, w, h, color);
  }
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                 uint8_t color) override {
    inner_->roundRect(x, y, w, h, r, color);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint8_t color) override {
    inner_->fillRoundRect(x, y, w, h, r, color);
  }
  void circle(int16_t x, int16_t y, int16_t r, uint8_t color) override {
    inner_->circle(x, y, r, color);
  }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint8_t color) override {
    inner_->fillCircle(x, y, r, color);
  }
  void ellipse(int16_t x, int16_t y, int16_t rw, int16_t rh, uint8_t color) override {
    inner_->ellipse(x, y, rw, rh, color);
  }
  void fillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh, uint8_t color) override {
    inner_->fillEllipse(x, y, rw, rh, color);
  }
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                uint8_t color) override {
    inner_->triangle(x0, y0, x1, y1, x2, y2, color);
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint8_t color) override {
    inner_->fillTriangle(x0, y0, x1, y1, x2, y2, color);
  }
  void rotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle,
                   uint8_t color) override {
    inner_->rotatedRect(cx, cy, w, h, angle, color);
  }
  void fillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle,
                       uint8_t color) override {
    inner_->fillRotatedRect(cx, cy, w, h, angle, color);
  }
  void bitmap(int16_t x, int16_t y, const uint8_t* bits) override {
    inner_->bitmap(x, y, bits);
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
  void glyph(int16_t x, int16_t y, uint8_t code, uint8_t size) override {
    inner_->glyph(x, y, code, size);
  }

 private:
  // The band's icons: Idle sun 0x0F, Pulling up triangle 0x1E, Holding inverse circle 0x0A,
  // Venting down triangle 0x1F, Fault X, the serial section sign 0x15 and the Bluetooth rune.
  void drawStateIcon();
  void drawConnections();
  void bluetoothIcon(int16_t x, int16_t y);
  void drawBattery();
  void batteryIcon(int16_t x, int16_t y, uint8_t percent);
  void emptyBatteryIcon(int16_t x, int16_t y);
  void drawUnit();

  Display* inner_;
  const Battery* battery_;
  const VacuumSystem* vacuum_;
  const Config* config_;
  bool serial_ = false;
  bool bluetooth_ = false;
};

}  // namespace gardener
