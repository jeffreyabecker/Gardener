#pragma once

#include <stdint.h>

#include "gfx/Canvas.h"

namespace gardener::gfx::shapes {

// The shape vocabulary: each shape is a value carrying its arguments that draws itself on the
// canvas it is given, through `canvas.setPixel` and nothing else. Clipping, the frame layout and a
// turned canvas are therefore the canvas's business, and a shape never asks where it is.
//
// Coordinates are relative to the canvas's top-left, `color` is the grayscale byte, and a shape
// whose size works out to nothing draws nothing.

// A line from one point to the other.
struct Line {
  Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
      : x0(x0), y0(y0), x1(x1), y1(y1), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
  uint8_t color;
};

// One row of pixels from (x, y) rightwards, `w` of them.
struct HLine {
  HLine(int16_t x, int16_t y, int16_t w, uint8_t color) : x(x), y(y), w(w), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t w;
  uint8_t color;
};

// One column of pixels from (x, y) down, `h` of them.
struct VLine {
  VLine(int16_t x, int16_t y, int16_t h, uint8_t color) : x(x), y(y), h(h), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t h;
  uint8_t color;
};

// The outline of a rectangle whose top-left pixel is (x, y).
struct Rect {
  Rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
      : x(x), y(y), w(w), h(h), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  uint8_t color;
};

// The same rectangle, filled.
struct FillRect {
  FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
      : x(x), y(y), w(w), h(h), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  uint8_t color;
};

// A rectangle with its corners rounded off, their radius clamped to half the shorter side so the
// arcs cannot cross.
struct RoundRect {
  RoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)
      : x(x), y(y), w(w), h(h), radius(radius), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  int16_t radius;
  uint8_t color;
};

// The same rounded rectangle, filled.
struct FillRoundRect {
  FillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)
      : x(x), y(y), w(w), h(h), radius(radius), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  int16_t radius;
  uint8_t color;
};

// The outline of a circle centred on (x, y).
struct Circle {
  Circle(int16_t x, int16_t y, int16_t radius, uint8_t color)
      : x(x), y(y), radius(radius), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t radius;
  uint8_t color;
};

// The same circle, filled.
struct FillCircle {
  FillCircle(int16_t x, int16_t y, int16_t radius, uint8_t color)
      : x(x), y(y), radius(radius), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t radius;
  uint8_t color;
};

// The outline of an ellipse centred on (x, y), `radiusX` across and `radiusY` down.
struct Ellipse {
  Ellipse(int16_t x, int16_t y, int16_t radiusX, int16_t radiusY, uint8_t color)
      : x(x), y(y), radiusX(radiusX), radiusY(radiusY), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t radiusX;
  int16_t radiusY;
  uint8_t color;
};

// The same ellipse, filled.
struct FillEllipse {
  FillEllipse(int16_t x, int16_t y, int16_t radiusX, int16_t radiusY, uint8_t color)
      : x(x), y(y), radiusX(radiusX), radiusY(radiusY), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  int16_t radiusX;
  int16_t radiusY;
  uint8_t color;
};

// The three edges of a triangle.
struct Triangle {
  Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
      : x0(x0), y0(y0), x1(x1), y1(y1), x2(x2), y2(y2), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  uint8_t color;
};

// The same triangle, filled.
struct FillTriangle {
  FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
               uint8_t color)
      : x0(x0), y0(y0), x1(x1), y1(y1), x2(x2), y2(y2), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  uint8_t color;
};

// A rectangle of `w` by `h` turned `angle` degrees clockwise about its centre, for a needle or a
// dial. The member is `angle` and not `degrees` because Arduino.h defines a degrees() macro, and a
// member of that name cannot be written in a TU that includes it.
struct RotatedRect {
  RotatedRect(int16_t centerX, int16_t centerY, int16_t w, int16_t h, int16_t angle,
              uint8_t color)
      : centerX(centerX), centerY(centerY), w(w), h(h), angle(angle), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t centerX;
  int16_t centerY;
  int16_t w;
  int16_t h;
  int16_t angle;
  uint8_t color;
};

// The same turned rectangle, filled.
struct FillRotatedRect {
  FillRotatedRect(int16_t centerX, int16_t centerY, int16_t w, int16_t h, int16_t angle,
                  uint8_t color)
      : centerX(centerX), centerY(centerY), w(w), h(h), angle(angle), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t centerX;
  int16_t centerY;
  int16_t w;
  int16_t h;
  int16_t angle;
  uint8_t color;
};

// An image the caller keeps: bits[0] is the width, bits[1] the height, then one grayscale byte per
// pixel, row-major. Every pixel is drawn; there is no transparency.
struct Bitmap {
  Bitmap(int16_t x, int16_t y, const uint8_t* bits) : x(x), y(y), bits(bits) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  const uint8_t* bits;
};

}  // namespace gardener::gfx::shapes
