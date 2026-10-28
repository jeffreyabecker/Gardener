#include "gfx/Shapes.h"

#include <math.h>
#include <string.h>

namespace gardener::gfx::shapes {
namespace {

void hline(Canvas& canvas, int16_t x, int16_t y, int16_t w, uint8_t color) {
  for (int16_t i = 0; i < w; i++) canvas.setPixel(static_cast<int16_t>(x + i), y, color);
}

void vline(Canvas& canvas, int16_t x, int16_t y, int16_t h, uint8_t color) {
  for (int16_t i = 0; i < h; i++) canvas.setPixel(x, static_cast<int16_t>(y + i), color);
}

// How far an ellipse reaches at `offset` from its centre along the axis of `radius`, measured on
// the axis that spans `other`.
int16_t halfExtent(int16_t radius, int16_t offset, int16_t other) {
  if (radius <= 0) return 0;
  const int32_t inside =
      static_cast<int32_t>(radius) * radius - static_cast<int32_t>(offset) * offset;
  if (inside <= 0) return 0;
  const float reach = sqrtf(static_cast<float>(inside)) / static_cast<float>(radius);
  return static_cast<int16_t>(lroundf(static_cast<float>(other) * reach));
}

// How far a rounded corner cuts in at `distance` rows or columns from the rectangle's edge.
int16_t cornerInset(int16_t radius, int16_t distance) {
  if (distance <= 0) return radius;
  if (distance >= radius) return 0;
  const int16_t reach = static_cast<int16_t>(radius - distance);
  return static_cast<int16_t>(radius - halfExtent(radius, reach, radius));
}

// The radius a rounded rectangle can actually use: half its shorter side.
int16_t clampRadius(int16_t w, int16_t h, int16_t radius) {
  const int16_t limit = static_cast<int16_t>((w < h ? w : h) / 2);
  return radius > limit ? limit : radius;
}

void ellipseFill(Canvas& canvas, int16_t cx, int16_t cy, int16_t radiusX, int16_t radiusY,
                 uint8_t color) {
  if (radiusX < 0 || radiusY < 0) return;
  for (int16_t dy = -radiusY; dy <= radiusY; dy++) {
    const int16_t half = halfExtent(radiusY, dy, radiusX);
    hline(canvas, static_cast<int16_t>(cx - half), static_cast<int16_t>(cy + dy),
          static_cast<int16_t>(2 * half + 1), color);
  }
}

void ellipseOutline(Canvas& canvas, int16_t cx, int16_t cy, int16_t radiusX, int16_t radiusY,
                    uint8_t color) {
  if (radiusX < 0 || radiusY < 0) return;
  // Both axes are walked: the rows alone leave gaps where the curve is steep, and the columns
  // alone leave them where it is flat.
  for (int16_t dy = -radiusY; dy <= radiusY; dy++) {
    const int16_t half = halfExtent(radiusY, dy, radiusX);
    canvas.setPixel(static_cast<int16_t>(cx - half), static_cast<int16_t>(cy + dy), color);
    canvas.setPixel(static_cast<int16_t>(cx + half), static_cast<int16_t>(cy + dy), color);
  }
  for (int16_t dx = -radiusX; dx <= radiusX; dx++) {
    const int16_t half = halfExtent(radiusX, dx, radiusY);
    canvas.setPixel(static_cast<int16_t>(cx + dx), static_cast<int16_t>(cy - half), color);
    canvas.setPixel(static_cast<int16_t>(cx + dx), static_cast<int16_t>(cy + half), color);
  }
}

void includeX(int16_t x, bool& any, int16_t& lo, int16_t& hi) {
  if (!any) {
    lo = x;
    hi = x;
    any = true;
    return;
  }
  if (x < lo) lo = x;
  if (x > hi) hi = x;
}

// The x a triangle edge crosses at scanline `y`, widened into lo and hi. A horizontal edge
// contributes both its ends, so the flat top or bottom of a triangle is not cut to a point.
void edgeAt(int16_t xa, int16_t ya, int16_t xb, int16_t yb, int16_t y, bool& any, int16_t& lo,
            int16_t& hi) {
  if ((y < ya && y < yb) || (y > ya && y > yb)) return;
  if (ya == yb) {
    includeX(xa, any, lo, hi);
    includeX(xb, any, lo, hi);
    return;
  }
  const int32_t x =
      static_cast<int32_t>(xa) + (static_cast<int32_t>(xb - xa) * (y - ya)) / (yb - ya);
  includeX(static_cast<int16_t>(x), any, lo, hi);
}

void triangleFill(Canvas& canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                  int16_t y2, uint8_t color) {
  int16_t top = y0 < y1 ? y0 : y1;
  if (y2 < top) top = y2;
  int16_t bottom = y0 > y1 ? y0 : y1;
  if (y2 > bottom) bottom = y2;

  for (int16_t y = top; y <= bottom; y++) {
    bool any = false;
    int16_t lo = 0;
    int16_t hi = 0;
    edgeAt(x0, y0, x1, y1, y, any, lo, hi);
    edgeAt(x1, y1, x2, y2, y, any, lo, hi);
    edgeAt(x2, y2, x0, y0, y, any, lo, hi);
    if (any) hline(canvas, lo, y, static_cast<int16_t>(hi - lo + 1), color);
  }
}

// The four corners of a rectangle turned about its centre, clockwise for a positive angle.
void turnedCorners(int16_t centerX, int16_t centerY, int16_t w, int16_t h, int16_t angle,
                   int16_t xs[4], int16_t ys[4]) {
  const float radians = static_cast<float>(angle) * 0.017453292519943295f;
  const float cosine = cosf(radians);
  const float sine = sinf(radians);
  const float halfW = static_cast<float>(w) / 2.0f;
  const float halfH = static_cast<float>(h) / 2.0f;
  const float cornerX[4] = {-halfW, halfW, halfW, -halfW};
  const float cornerY[4] = {-halfH, -halfH, halfH, halfH};
  for (int i = 0; i < 4; i++) {
    const float px = cornerX[i] * cosine - cornerY[i] * sine;
    const float py = cornerX[i] * sine + cornerY[i] * cosine;
    xs[i] = static_cast<int16_t>(lroundf(static_cast<float>(centerX) + px));
    ys[i] = static_cast<int16_t>(lroundf(static_cast<float>(centerY) + py));
  }
}

// One hex digit as the value it names, or -1 for a character that is not one.
int hexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}

// The byte two digits name, which the caller has already read as hex.
uint8_t hexByte(const char* text) {
  return static_cast<uint8_t>((hexDigit(text[0]) << 4) | hexDigit(text[1]));
}

}  // namespace

void Line::operator()(Canvas& canvas) const {
  int16_t x = x0;
  int16_t y = y0;
  const int16_t dx = static_cast<int16_t>(x1 > x0 ? x1 - x0 : x0 - x1);
  const int16_t dy = static_cast<int16_t>(y1 > y0 ? y0 - y1 : y1 - y0);
  const int16_t stepX = x0 < x1 ? 1 : -1;
  const int16_t stepY = y0 < y1 ? 1 : -1;
  int16_t error = static_cast<int16_t>(dx + dy);
  for (;;) {
    canvas.setPixel(x, y, color);
    if (x == x1 && y == y1) break;
    const int16_t doubled = static_cast<int16_t>(2 * error);
    if (doubled >= dy) {
      error = static_cast<int16_t>(error + dy);
      x = static_cast<int16_t>(x + stepX);
    }
    if (doubled <= dx) {
      error = static_cast<int16_t>(error + dx);
      y = static_cast<int16_t>(y + stepY);
    }
  }
}

void HLine::operator()(Canvas& canvas) const { hline(canvas, x, y, w, color); }

void VLine::operator()(Canvas& canvas) const { vline(canvas, x, y, h, color); }

void Rect::operator()(Canvas& canvas) const {
  if (w <= 0 || h <= 0) return;
  hline(canvas, x, y, w, color);
  hline(canvas, x, static_cast<int16_t>(y + h - 1), w, color);
  vline(canvas, x, y, h, color);
  vline(canvas, static_cast<int16_t>(x + w - 1), y, h, color);
}

void FillRect::operator()(Canvas& canvas) const {
  for (int16_t row = 0; row < h; row++) {
    hline(canvas, x, static_cast<int16_t>(y + row), w, color);
  }
}

void RoundRect::operator()(Canvas& canvas) const {
  if (w <= 0 || h <= 0) return;
  const int16_t r = clampRadius(w, h, radius);
  if (r <= 0) {
    Rect(x, y, w, h, color)(canvas);
    return;
  }

  hline(canvas, static_cast<int16_t>(x + r), y, static_cast<int16_t>(w - 2 * r), color);
  hline(canvas, static_cast<int16_t>(x + r), static_cast<int16_t>(y + h - 1),
        static_cast<int16_t>(w - 2 * r), color);
  vline(canvas, x, static_cast<int16_t>(y + r), static_cast<int16_t>(h - 2 * r), color);
  vline(canvas, static_cast<int16_t>(x + w - 1), static_cast<int16_t>(y + r),
        static_cast<int16_t>(h - 2 * r), color);

  for (int16_t step = 0; step < r; step++) {
    const int16_t inset = cornerInset(r, step);
    const int16_t nearLeft = static_cast<int16_t>(x + inset);
    const int16_t nearRight = static_cast<int16_t>(x + w - 1 - inset);
    const int16_t nearTop = static_cast<int16_t>(y + step);
    const int16_t nearBottom = static_cast<int16_t>(y + h - 1 - step);
    canvas.setPixel(nearLeft, nearTop, color);
    canvas.setPixel(nearRight, nearTop, color);
    canvas.setPixel(nearLeft, nearBottom, color);
    canvas.setPixel(nearRight, nearBottom, color);

    const int16_t fromLeft = static_cast<int16_t>(x + step);
    const int16_t fromRight = static_cast<int16_t>(x + w - 1 - step);
    const int16_t fromTop = static_cast<int16_t>(y + inset);
    const int16_t fromBottom = static_cast<int16_t>(y + h - 1 - inset);
    canvas.setPixel(fromLeft, fromTop, color);
    canvas.setPixel(fromRight, fromTop, color);
    canvas.setPixel(fromLeft, fromBottom, color);
    canvas.setPixel(fromRight, fromBottom, color);
  }
}

void FillRoundRect::operator()(Canvas& canvas) const {
  if (w <= 0 || h <= 0) return;
  const int16_t r = clampRadius(w, h, radius);
  if (r <= 0) {
    FillRect(x, y, w, h, color)(canvas);
    return;
  }

  for (int16_t step = 0; step < r; step++) {
    const int16_t inset = cornerInset(r, step);
    const int16_t span = static_cast<int16_t>(w - 2 * inset);
    hline(canvas, static_cast<int16_t>(x + inset), static_cast<int16_t>(y + step), span, color);
    hline(canvas, static_cast<int16_t>(x + inset), static_cast<int16_t>(y + h - 1 - step), span,
          color);
  }
  for (int16_t row = r; row < h - r; row++) {
    hline(canvas, x, static_cast<int16_t>(y + row), w, color);
  }
}

void Circle::operator()(Canvas& canvas) const {
  ellipseOutline(canvas, x, y, radius, radius, color);
}

void FillCircle::operator()(Canvas& canvas) const {
  ellipseFill(canvas, x, y, radius, radius, color);
}

void Ellipse::operator()(Canvas& canvas) const {
  ellipseOutline(canvas, x, y, radiusX, radiusY, color);
}

void FillEllipse::operator()(Canvas& canvas) const {
  ellipseFill(canvas, x, y, radiusX, radiusY, color);
}

void Triangle::operator()(Canvas& canvas) const {
  Line(x0, y0, x1, y1, color)(canvas);
  Line(x1, y1, x2, y2, color)(canvas);
  Line(x2, y2, x0, y0, color)(canvas);
}

void FillTriangle::operator()(Canvas& canvas) const {
  triangleFill(canvas, x0, y0, x1, y1, x2, y2, color);
}

void RotatedRect::operator()(Canvas& canvas) const {
  if (w <= 0 || h <= 0) return;
  int16_t xs[4];
  int16_t ys[4];
  turnedCorners(centerX, centerY, w, h, angle, xs, ys);
  for (int i = 0; i < 4; i++) {
    const int next = (i + 1) % 4;
    Line(xs[i], ys[i], xs[next], ys[next], color)(canvas);
  }
}

void FillRotatedRect::operator()(Canvas& canvas) const {
  if (w <= 0 || h <= 0) return;
  int16_t xs[4];
  int16_t ys[4];
  turnedCorners(centerX, centerY, w, h, angle, xs, ys);
  triangleFill(canvas, xs[0], ys[0], xs[1], ys[1], xs[2], ys[2], color);
  triangleFill(canvas, xs[0], ys[0], xs[2], ys[2], xs[3], ys[3], color);
}

void MonoBitmap::operator()(Canvas& canvas) const {
  const int16_t width = bits[0];
  const int16_t height = bits[1];
  const int16_t stride = static_cast<int16_t>((width + 7) / 8);
  const uint8_t* rows = bits + 2;
  for (int16_t row = 0; row < height; row++) {
    const uint8_t* line = rows + row * stride;
    for (int16_t col = 0; col < width; col++) {
      if ((line[col / 8] & (0x80 >> (col & 7))) != 0) {
        canvas.setPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), color);
      }
    }
  }
}

bool isMonoBitmap(const char* text) {
  if (text == nullptr) return false;
  const size_t count = strlen(text);
  if (count < 4 || count % 2 != 0) return false;
  for (size_t i = 0; i < count; i++) {
    if (hexDigit(text[i]) < 0) return false;
  }

  const uint8_t width = hexByte(text);
  const uint8_t height = hexByte(text + 2);
  if (width == 0 || height == 0) return false;
  const size_t rows = ((size_t)width + 7) / 8 * height;
  return count / 2 >= 2 + rows;
}

std::vector<uint8_t> monoBitmapBits(const char* text) {
  std::vector<uint8_t> bits;
  if (!isMonoBitmap(text)) return bits;

  const size_t count = strlen(text);
  bits.reserve(count / 2);
  for (size_t i = 0; i < count; i += 2) bits.push_back(hexByte(text + i));
  return bits;
}

void Bitmap::operator()(Canvas& canvas) const {
  const int16_t width = bits[0];
  const int16_t height = bits[1];
  for (int16_t row = 0; row < height; row++) {
    for (int16_t col = 0; col < width; col++) {
      const int32_t index = 2 + static_cast<int32_t>(row) * width + col;
      canvas.setPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), bits[index]);
    }
  }
}

}  // namespace gardener::gfx::shapes
