#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <Gfx.h>

namespace gardener {

// The colour a drawing verb takes: 0 is paper, 1 is ink, the values the drivers read. A verb takes a
// uint8_t, so a caller writes one of these two.
constexpr uint8_t Paper = 0;
constexpr uint8_t Ink = 1;

// A monochrome panel: the surface a page draws on, with the panel itself behind it.
//
// Two rules shape what is here.
//
// Colours are a choice per call, so paper on its own is a fill like any other, and `inverted` is
// how layout code asks for paper on ink in text.
//
// Otherwise the list is Gfx's drawing surface in those terms, so that whatever draws on this later
// does not have to come back and widen it. Deliberately absent, with reasons: the grayscale and RGB
// bitmap shapes need a depth this panel does not have. The font face is the one library type named
// here, because a face is data the drawing picks rather than a part a driver hides: a host or a
// fake draws through the same library and takes its faces from the same table.
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

  // The frame those verbs write, for a page composed with Gfx's own canvas: the same buffer and the
  // same record of what was written, so a canvas draw and a verb draw are sent by one display().
  virtual gfx::Canvas& canvas() = 0;

  // --- geometry ------------------------------------------------------------
  // Rotation aware: width() and height() swap when the panel is turned.
  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;
  // The area a page lays out within. On a plain panel this is the whole glass; a wrapper that
  // reserves a band of its own (the overlay's status row) narrows it, so a page reads these rather
  // than subtracting a band it does not own.
  virtual int16_t pageWidth() const = 0;
  virtual int16_t pageHeight() const = 0;
  virtual uint8_t rotation() const = 0;
  virtual void setRotation(uint8_t rotation) = 0;

  // --- the screen ----------------------------------------------------------
  virtual void clear(uint8_t color) = 0;  // one colour everywhere
  virtual void setInverted(bool on) = 0;  // the whole panel, not the next thing drawn
  // What setInverted() was last given. The wire reports it — `panel get` has to answer with the
  // state the panel is in, and a host that attaches mid-session has no other way to learn it.
  virtual bool inverted() const = 0;

  // --- pixels, lines and shapes --------------------------------------------
  // Panel pixels, origin top left. The colour is the call's, so a shape in paper erases.
  virtual void pixel(int16_t x, int16_t y, uint8_t color) = 0;
  // Flips what is already there — every pixel in the rectangle goes from ink to paper or paper to
  // ink — which is how a line drawn in a proportional face gets a paper-on-ink highlight, and how
  // a track laid over a border keeps the border visible. It draws, so it lands in the frame buffer
  // with the rest and waits for display(); setInverted() is the panel's own command and does not.
  virtual void invert(int16_t x, int16_t y, int16_t w, int16_t h) = 0;
  virtual void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) = 0;
  virtual void hLine(int16_t x, int16_t y, int16_t w, uint8_t color) = 0;
  virtual void vLine(int16_t x, int16_t y, int16_t h, uint8_t color) = 0;
  virtual void rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) = 0;
  virtual void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                         uint8_t color) = 0;
  virtual void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                             uint8_t color) = 0;
  virtual void circle(int16_t x, int16_t y, int16_t r, uint8_t color) = 0;
  virtual void fillCircle(int16_t x, int16_t y, int16_t r, uint8_t color) = 0;
  virtual void ellipse(int16_t x, int16_t y, int16_t rw, int16_t rh, uint8_t color) = 0;
  virtual void fillEllipse(int16_t x, int16_t y, int16_t rw, int16_t rh, uint8_t color) = 0;
  virtual void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        uint8_t color) = 0;
  virtual void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                            int16_t y2, uint8_t color) = 0;
  // Turned about (cx, cy) by `angle` degrees, for a needle or a dial.
  virtual void rotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle,
                           uint8_t color) = 0;
  virtual void fillRotatedRect(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t angle,
                               uint8_t color) = 0;

  // A packed one-bit image: bits[0] is the width, bits[1] the height, then the pixels row-major
  // and most-significant bit first, each row padded to a whole byte. Ink bits are drawn; paper
  // bits are left as whatever is under them.
  virtual void bitmap(int16_t x, int16_t y, const uint8_t* bits) = 0;

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
  virtual void setFont(const gfx::Font* font) = 0;
  virtual void text(int16_t x, int16_t y, const String& value, uint8_t size, bool inverted) = 0;
  virtual void text(int16_t x, int16_t y, const char* value, uint8_t size, bool inverted) = 0;
  virtual void textBounds(const String& value, uint8_t size, int16_t& outWidth,
                          int16_t& outHeight) = 0;
  virtual void textBounds(const char* value, uint8_t size, int16_t& outWidth,
                          int16_t& outHeight) = 0;

  // One glyph of the built-in 5x7 face, by its code rather than as text. The band's state icons
  // live in the face's control range, where a string would have to carry them past the NUL, so a
  // raw draw is the way to put one down.
  virtual void glyph(int16_t x, int16_t y, uint8_t code, uint8_t size) = 0;
};

}  // namespace gardener
