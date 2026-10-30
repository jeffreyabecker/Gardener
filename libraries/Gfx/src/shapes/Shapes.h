#pragma once

#include <stdint.h>

#include <vector>

#include "../Canvas.h"

namespace gardener::gfx::shapes {

// The shape vocabulary: each shape is a value carrying its arguments that draws itself on the
// canvas it is given, through `canvas.setPixel` and nothing else. Clipping, the frame layout and a
// turned canvas are therefore the canvas's business, and a shape never asks where it is.
//
// Coordinates are relative to the canvas's top-left, `color` is the grayscale byte, `filled` picks
// the solid form over the outline and is false for one, and a shape whose size works out to nothing
// draws nothing.

// A line from one point to the other.
struct Line {
  Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
      : x0(x0), y0(y0), x1(x1), y1(y1), color(color) {}

  // The line from (x, y) rightwards over `w` pixels, which is one pixel for a `w` of one or less.
  static Line horizontal(int16_t x, int16_t y, int16_t w, uint8_t color) {
    return Line(x, y, static_cast<int16_t>(x + (w > 0 ? w - 1 : 0)), y, color);
  }

  // The same line downwards over `h` pixels.
  static Line vertical(int16_t x, int16_t y, int16_t h, uint8_t color) {
    return Line(x, y, x, static_cast<int16_t>(y + (h > 0 ? h - 1 : 0)), color);
  }

  void operator()(Canvas& canvas) const;

  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
  uint8_t color;
};

// A rectangle whose top-left pixel is (x, y), its corners rounded off by `radius` when that is
// positive, the radius clamped to half the shorter side so the arcs cannot cross.
struct Rect {
  Rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color, bool filled = false,
       int16_t radius = 0)
      : x(x), y(y), w(w), h(h), color(color), filled(filled), radius(radius) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  uint8_t color;
  bool filled;
  int16_t radius;
};

// A circle centred on (x, y).
struct Circle {
  Circle(int16_t x, int16_t y, int16_t radius, uint8_t color, bool filled = false)
      : x(x), y(y), radius(radius), color(color), filled(filled) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t radius;
  uint8_t color;
  bool filled;
};

// An ellipse centred on (x, y), `radiusX` across and `radiusY` down.
struct Ellipse {
  Ellipse(int16_t x, int16_t y, int16_t radiusX, int16_t radiusY, uint8_t color,
          bool filled = false)
      : x(x), y(y), radiusX(radiusX), radiusY(radiusY), color(color), filled(filled) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t radiusX;
  int16_t radiusY;
  uint8_t color;
  bool filled;
};

// A triangle through its three corners.
struct Triangle {
  Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color,
           bool filled = false)
      : x0(x0), y0(y0), x1(x1), y1(y1), x2(x2), y2(y2), color(color), filled(filled) {}

  void operator()(Canvas& canvas) const;

  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  uint8_t color;
  bool filled;
};

// Nothing draws a turned rectangle yet — a needle or a dial is the caller that will first want
// one — so the two forms and the corner arithmetic they share stay in comment until it arrives.
//
// // A rectangle of `w` by `h` turned `angle` degrees clockwise about its centre, for a needle or a
// // dial. The member is `angle` and not `degrees` because Arduino.h defines a degrees() macro, and
// // a member of that name cannot be written in a TU that includes it.
// struct RotatedRect {
//   RotatedRect(int16_t centerX, int16_t centerY, int16_t w, int16_t h, int16_t angle,
//               uint8_t color)
//       : centerX(centerX), centerY(centerY), w(w), h(h), angle(angle), color(color) {}
//
//   void operator()(Canvas& canvas) const;
//
//   int16_t centerX;
//   int16_t centerY;
//   int16_t w;
//   int16_t h;
//   int16_t angle;
//   uint8_t color;
// };
//
// // The same turned rectangle, filled.
// struct FillRotatedRect {
//   FillRotatedRect(int16_t centerX, int16_t centerY, int16_t w, int16_t h, int16_t angle,
//                   uint8_t color)
//       : centerX(centerX), centerY(centerY), w(w), h(h), angle(angle), color(color) {}
//
//   void operator()(Canvas& canvas) const;
//
//   int16_t centerX;
//   int16_t centerY;
//   int16_t w;
//   int16_t h;
//   int16_t angle;
//   uint8_t color;
// };

// A packed one-bit image the caller keeps: bits[0] is the width, bits[1] the height, then the rows
// most-significant bit first, each padded to a whole byte. A set bit draws `color`; a clear bit
// leaves what is under it.
struct MonoBitmap {
  MonoBitmap(int16_t x, int16_t y, const uint8_t* bits, uint8_t color)
      : x(x), y(y), bits(bits), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  const uint8_t* bits;
  uint8_t color;
};

// Whether a string names a MonoBitmap: hex digits, two per byte, the width, the height, then one row
// per byte-run, most significant bit first and each row padded to a whole byte. Nothing is
// allocated, so a caller that refuses a string it cannot read has nothing to keep.
bool isMonoBitmap(const char* text);

// The bytes a string that names a MonoBitmap holds, ready for MonoBitmap to read, and nothing for a
// string that is not one.
std::vector<uint8_t> monoBitmapBits(const char* text);

// The panel draws MonoBitmap and nothing draws one grayscale byte per pixel, so this form stays in
// comment until an image that needs more than ink and paper arrives.
//
// // An image the caller keeps: bits[0] is the width, bits[1] the height, then one grayscale byte
// // per pixel, row-major. Every pixel is drawn; there is no transparency.
// struct Bitmap {
//   Bitmap(int16_t x, int16_t y, const uint8_t* bits) : x(x), y(y), bits(bits) {}
//
//   void operator()(Canvas& canvas) const;
//
//   int16_t x;
//   int16_t y;
//   const uint8_t* bits;
// };

}  // namespace gardener::gfx::shapes
