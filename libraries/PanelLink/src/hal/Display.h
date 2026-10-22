#pragma once

#include <Adafruit_GFX.h>
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
// drawGrayscaleBitmap and drawRGBBitmap need a depth this panel does not have. The font face is the
// one library type named here, because a face is data the drawing picks rather than a part a driver
// hides: a host or a fake needs the GFX header and nothing else.
//
// Text is nearly stateless: `text` and `textBounds` take the scale they draw at, so nothing has to
// park a cursor or a size and remember to undo them. The face is the exception — setFont() picks it
// once for the page being laid out, since a page draws many strings in one face and no single
// string owns it.
class Display {
 public:
  virtual ~Display() = default;

  // False when the panel did not answer; nothing draws until this returns true.
  virtual bool begin() = 0;

  // --- the panel -----------------------------------------------------------
  // Drawing lands in the panel's own frame buffer, and display() sends that buffer to the glass.
  // There is no bracket around a frame: the buffer keeps what was drawn until the next clear() or
  // draw, so the only thing that ever has to run is the flush.
  virtual void display() = 0;  // push the frame buffer to the glass

  // --- geometry ------------------------------------------------------------
  // Rotation aware: width() and height() swap when the panel is turned.
  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;
  virtual uint8_t rotation() const = 0;
  virtual void setRotation(uint8_t rotation) = 0;

  // --- the screen ----------------------------------------------------------
  virtual void clear() = 0;               // paper everywhere
  virtual void setInverted(bool on) = 0;  // the whole panel, not the next thing drawn
  // What setInverted() was last given. The wire reports it — `panel get` has to answer with the
  // state the panel is in, and a host that attaches mid-session has no other way to learn it.
  virtual bool inverted() const = 0;

  // --- pixels, lines and shapes --------------------------------------------
  // Panel pixels, origin top left. Ink unless the name says otherwise.
  virtual void pixel(int16_t x, int16_t y) = 0;
  virtual void clearRect(int16_t x, int16_t y, int16_t w, int16_t h) = 0;
  // Flips what is already there — every pixel in the rectangle goes from ink to paper or paper to
  // ink — which is how a line drawn in a proportional face gets a paper-on-ink highlight, and how
  // a track laid over a border keeps the border visible. It draws, so it lands in the frame buffer
  // with the rest and waits for display(); setInverted() is the panel's own command and does not.
  virtual void invert(int16_t x, int16_t y, int16_t w, int16_t h) = 0;
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

  // --- text ----------------------------------------------------------------
  // The face is chosen once and stays chosen; setFont(nullptr) puts the built-in 5x7 back. With a
  // face set, `y` is the baseline rather than the top of the glyph box, which is the font format's
  // own rule and the one thing a caller has to know about it.
  //
  // `size` scales whichever face is set, and `inverted` draws paper on ink — but only the built-in
  // face can: the library draws a custom face with no background at all, so an inverted string in
  // one is ink on whatever was already there. Measurement is a question about a string at a size
  // rather than a property of wherever the cursor happens to be, and it is not const because the
  // library's own measurement is not.
  //
  // Both spellings exist because both callers do: a host message arrives as a `const char*` in a
  // line buffer, and anything the device builds is a `String`. One of them would otherwise be
  // converted on every call for nothing.
  virtual void setFont(const GFXfont* font) = 0;
  virtual void text(int16_t x, int16_t y, const String& value, uint8_t size, bool inverted) = 0;
  virtual void text(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) = 0;
  virtual void textBounds(const String& value, uint8_t size, int16_t& outWidth,
                          int16_t& outHeight) = 0;
  virtual void textBounds(const char* value, uint8_t size, int16_t& outWidth,
                          int16_t& outHeight) = 0;
};

}  // namespace gardener
