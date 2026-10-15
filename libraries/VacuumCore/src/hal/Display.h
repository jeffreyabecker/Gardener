#pragma once

#include <Arduino.h>
#include <Print.h>
#include <stdint.h>

namespace eggplan {

// A monochrome panel: the surface a page draws on, with the panel itself behind it.
//
// Two rules shape what is here.
//
// Colours belong to the implementation, so no verb takes one. Ink is what the drawing and
// filling verbs put down, `inverted` is how layout code asks for paper on ink, and paper on its
// own has two spellings: clear() for the whole panel, clearRect() for part of it. Nothing here
// names the numbers, because nothing here should have to know that 1 lights a pixel.
//
// Otherwise the list is the library's drawing surface in those terms, so that a page written
// later does not have to come back and widen it. Deliberately absent, with reasons:
// drawGrayscaleBitmap and drawRGBBitmap need a depth this panel does not have; the
// startWrite/writePixel family exists to batch pin work on an SPI panel and does nothing on
// I2C; setFont takes a `GFXfont*`, and a header that names the library's types is the one thing
// this abstraction exists to avoid.
//
// Text keeps both spellings the library has: the stateful one (setTextSize, setCursor,
// setTextWrap, then print, through Print) and the state free one (text, textWidth, textBounds)
// that layout code wants.
class Display : public Print {
 public:
  virtual ~Display() = default;

  // --- the panel -----------------------------------------------------------
  // False when no panel answered. Drawing before this succeeds is not an error, it is just
  // invisible.
  virtual bool begin() = 0;
  virtual void flush() = 0;  // push what has been drawn to the glass

  // --- geometry ------------------------------------------------------------
  // Rotation aware: width() and height() swap when the panel is turned.
  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;
  virtual uint8_t rotation() const = 0;
  virtual void setRotation(uint8_t rotation) = 0;

  // --- the screen ----------------------------------------------------------
  virtual void clear() = 0;          // paper everywhere
  virtual void invert(bool on) = 0;  // the whole panel, not the next thing drawn

  // --- pixels, lines and shapes --------------------------------------------
  // Panel pixels, origin top left. Ink unless the name says otherwise.
  virtual void pixel(int16_t x, int16_t y) = 0;
  virtual void clearRect(int16_t x, int16_t y, int16_t w, int16_t h) = 0;
  virtual void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) = 0;
  virtual void hLine(int16_t x, int16_t y, int16_t w) = 0;
  virtual void vLine(int16_t x, int16_t y, int16_t h) = 0;
  virtual void rect(int16_t x, int16_t y, int16_t w, int16_t h) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h) = 0;
  virtual void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) = 0;
  virtual void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) = 0;
  virtual void circle(int16_t x, int16_t y, int16_t r) = 0;
  virtual void fillCircle(int16_t x, int16_t y, int16_t r) = 0;
  virtual void ellipse(int16_t x, int16_t y, int16_t rw, int16_t rh) = 0;
  virtual void fillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh) = 0;
  virtual void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                        int16_t y2) = 0;
  virtual void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                            int16_t y2) = 0;
  // Turned about (cx, cy) by `angle` degrees, for a needle or a dial.
  virtual void rotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle) = 0;
  virtual void fillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h,
                               int16_t angle) = 0;

  // --- images --------------------------------------------------------------
  // One bit per pixel, row major, most significant bit first, in program memory. A set bit is
  // ink. `xBitmap` reads the same layout with the bit order reversed, for a tool that emits it
  // that way.
  virtual void bitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h) = 0;
  virtual void xBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h) = 0;

  // --- text state ----------------------------------------------------------
  // Set a size, park the cursor, print. `print` and its whole family come from Print, so a
  // number does not have to be built into a string first.
  virtual void setTextSize(uint8_t size) = 0;
  virtual void setTextSize(uint8_t sx, uint8_t sy) = 0;
  virtual void setCursor(int16_t x, int16_t y) = 0;
  virtual void setTextWrap(bool wrap) = 0;
  virtual void cp437(bool on) = 0;

  // --- text, without the state --------------------------------------------
  // `size` is the text scale and `inverted` draws paper on ink. Measurement is a question about
  // a string at a size rather than a property of wherever the cursor happens to be. Neither is
  // const, because the library's own measurement is not.
  virtual void text(int16_t x, int16_t y, const String& value, uint8_t size, bool inverted) = 0;
  virtual int16_t textWidth(const String& value, uint8_t size) = 0;
  // Both dimensions, for stacking rows: height is not always the scale times eight once a
  // bitmap font is in play, which is why it is asked for rather than assumed.
  virtual void textBounds(const String& value, uint8_t size, int16_t& outWidth,
                          int16_t& outHeight) = 0;
};

}  // namespace eggplan
