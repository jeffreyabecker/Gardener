#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace gardener {

// A monochrome panel: the surface a page draws on, with the panel itself behind it.
//
// Two rules shape what is here.
//
// Colours belong to the implementation, so no verb takes one. Ink is what the drawing and
// filling verbs put down, `inverted` is how layout code asks for paper on ink, and paper on its
// own has two spellings: clear() for the whole panel, clearRect() for part of it. Nothing here
// names the numbers, because nothing here should have to know that 1 lights a pixel.
//
// Otherwise the list is the library's drawing surface in those terms, so that whatever draws on
// this later does not have to come back and widen it. Deliberately absent, with reasons:
// drawGrayscaleBitmap and drawRGBBitmap need a depth this panel does not have, and the
// startWrite/writePixel family exists to batch pin work on an SPI panel and does nothing on
// I2C. Nothing here names a library type at all, so a host and a fake can implement this with no
// Adafruit in the picture.
//
// Text is stateless: `text` and `textBounds` take the scale they draw at, so nothing has to park
// a cursor, set a size and remember to undo it. The built-in font is the only one there is.
class Display {
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
  // ink.
  virtual void bitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h) = 0;

  // --- text ----------------------------------------------------------------
  // `size` scales the built-in font, and `inverted` draws paper on ink. Measurement is a question
  // about a string at a size rather than a property of wherever the cursor happens to be, and it
  // is not const because the library's own measurement is not.
  //
  // Both spellings exist because both callers do: a host message arrives as a `const char*` in a
  // line buffer, and anything the device builds is a `String`. One of them would otherwise be
  // converted on every call for nothing.
  virtual void text(int16_t x, int16_t y, const String& value, uint8_t size, bool inverted) = 0;
  virtual void text(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) = 0;
  virtual void textBounds(const String& value, uint8_t size, int16_t& outWidth,
                          int16_t& outHeight) = 0;
  virtual void textBounds(const char* value, uint8_t size, int16_t& outWidth,
                          int16_t& outHeight) = 0;
};

}  // namespace gardener
