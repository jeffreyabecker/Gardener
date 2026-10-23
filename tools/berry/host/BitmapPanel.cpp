#include "BitmapPanel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The classic 5x7 face the built-in font draws with. The custom faces come from Fonts.cpp, which
// compiles the library's own headers; this is the only data the panel needs of its own.
#include <glcdfont.c>

namespace {
constexpr float kPi = 3.14159265f;
constexpr uint16_t kInk = 1;
}  // namespace

BitmapPanel::BitmapPanel(int16_t width, int16_t height) : width_(width), height_(height) {}

bool BitmapPanel::begin() {
  fillScreen(0);
  return true;
}

void BitmapPanel::fillScreen(uint16_t color) {
  memset(frame_, color & 1, sizeof frame_);
}

// --- pixels, the way Adafruit_GFX puts them down ---------------------------

void BitmapPanel::drawPixel(int16_t x, int16_t y, uint16_t color) {
  switch (rotation_) {
    case 1: {
      const int16_t swap = x;
      x = y;
      y = swap;
      x = (int16_t)(width_ - 1 - x);
      break;
    }
    case 2:
      x = (int16_t)(width_ - 1 - x);
      y = (int16_t)(height_ - 1 - y);
      break;
    case 3: {
      const int16_t swap = x;
      x = y;
      y = swap;
      y = (int16_t)(height_ - 1 - y);
      break;
    }
    default:
      break;
  }
  writePixel(x, y, color);
}

void BitmapPanel::writePixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  // 2 is the monochrome drivers' "toggle this pixel", which is what a region invert draws with.
  frame_[y][x] = color == 2 ? (uint8_t)!frame_[y][x] : (uint8_t)(color & 1);
}

void BitmapPanel::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    int16_t swap = x0;
    x0 = y0;
    y0 = swap;
    swap = x1;
    x1 = y1;
    y1 = swap;
  }
  if (x0 > x1) {
    int16_t swap = x0;
    x0 = x1;
    x1 = swap;
    swap = y0;
    y0 = y1;
    y1 = swap;
  }

  const int16_t dx = x1 - x0;
  const int16_t dy = abs(y1 - y0);
  int16_t err = dx / 2;
  const int16_t ystep = (y0 < y1) ? 1 : -1;

  for (; x0 <= x1; x0++) {
    if (steep) {
      writePixel(y0, x0, color);
    } else {
      writePixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void BitmapPanel::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  writeLine(x, y, x + w - 1, y, color);
}

void BitmapPanel::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  writeLine(x, y, x, y + h - 1, color);
}

void BitmapPanel::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t i = x; i < x + w; i++) writeFastVLine(i, y, h, color);
}

void BitmapPanel::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  if (x0 == x1) {
    if (y0 > y1) {
      const int16_t swap = y0;
      y0 = y1;
      y1 = swap;
    }
    writeFastVLine(x0, y0, y1 - y0 + 1, color);
  } else if (y0 == y1) {
    if (x0 > x1) {
      const int16_t swap = x0;
      x0 = x1;
      x1 = swap;
    }
    writeFastHLine(x0, y0, x1 - x0 + 1, color);
  } else {
    writeLine(x0, y0, x1, y1, color);
  }
}

void BitmapPanel::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  writeLine(x, y, x + w - 1, y, color);
}

void BitmapPanel::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  writeLine(x, y, x, y + h - 1, color);
}

void BitmapPanel::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t i = x; i < x + w; i++) writeFastVLine(i, y, h, color);
}

void BitmapPanel::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  writeFastHLine(x, y, w, color);
  writeFastHLine(x, y + h - 1, w, color);
  writeFastVLine(x, y, h, color);
  writeFastVLine(x + w - 1, y, h, color);
}

void BitmapPanel::drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners,
                                   uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    if (corners & 0x4) {
      writePixel(x0 + x, y0 + y, color);
      writePixel(x0 + y, y0 + x, color);
    }
    if (corners & 0x2) {
      writePixel(x0 + x, y0 - y, color);
      writePixel(x0 + y, y0 - x, color);
    }
    if (corners & 0x8) {
      writePixel(x0 - y, y0 + x, color);
      writePixel(x0 - x, y0 + y, color);
    }
    if (corners & 0x1) {
      writePixel(x0 - y, y0 - x, color);
      writePixel(x0 - x, y0 - y, color);
    }
  }
}

void BitmapPanel::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  writePixel(x0, y0 + r, color);
  writePixel(x0, y0 - r, color);
  writePixel(x0 + r, y0, color);
  writePixel(x0 - r, y0, color);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    writePixel(x0 + x, y0 + y, color);
    writePixel(x0 - x, y0 + y, color);
    writePixel(x0 + x, y0 - y, color);
    writePixel(x0 - x, y0 - y, color);
    writePixel(x0 + y, y0 + x, color);
    writePixel(x0 - y, y0 + x, color);
    writePixel(x0 + y, y0 - x, color);
    writePixel(x0 - y, y0 - x, color);
  }
}

void BitmapPanel::fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners,
                                   int16_t delta, uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;
  int16_t px = x;
  int16_t py = y;

  delta++;

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    if (x < (y + 1)) {
      if (corners & 1) writeFastVLine(x0 + x, y0 - y, 2 * y + delta, color);
      if (corners & 2) writeFastVLine(x0 - x, y0 - y, 2 * y + delta, color);
    }
    if (y != py) {
      if (corners & 1) writeFastVLine(x0 + py, y0 - px, 2 * px + delta, color);
      if (corners & 2) writeFastVLine(x0 - py, y0 - px, 2 * px + delta, color);
      py = y;
    }
    px = x;
  }
}

void BitmapPanel::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  writeFastVLine(x0, y0 - r, 2 * r + 1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

void BitmapPanel::drawEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color) {
  int16_t x = 0, y = rh;
  const int32_t rw2 = rw * rw, rh2 = rh * rh;
  const int32_t twoRw2 = 2 * rw2, twoRh2 = 2 * rh2;
  int32_t decision = rh2 - (rw2 * rh) + (rw2 / 4);

  while ((twoRh2 * x) < (twoRw2 * y)) {
    writePixel(x0 + x, y0 + y, color);
    writePixel(x0 - x, y0 + y, color);
    writePixel(x0 + x, y0 - y, color);
    writePixel(x0 - x, y0 - y, color);
    x++;
    if (decision < 0) {
      decision += rh2 + (twoRh2 * x);
    } else {
      decision += rh2 + (twoRh2 * x) - (twoRw2 * y);
      y--;
    }
  }

  decision = ((rh2 * (2 * x + 1) * (2 * x + 1)) >> 2) + (rw2 * (y - 1) * (y - 1)) - (rw2 * rh2);
  while (y >= 0) {
    writePixel(x0 + x, y0 + y, color);
    writePixel(x0 - x, y0 + y, color);
    writePixel(x0 + x, y0 - y, color);
    writePixel(x0 - x, y0 - y, color);
    y--;
    if (decision > 0) {
      decision += rw2 - (twoRw2 * y);
    } else {
      decision += rw2 + (twoRh2 * x) - (twoRw2 * y);
      x++;
    }
  }
}

void BitmapPanel::fillEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color) {
  int16_t x = 0, y = rh;
  const int32_t rw2 = rw * rw, rh2 = rh * rh;
  const int32_t twoRw2 = 2 * rw2, twoRh2 = 2 * rh2;
  int32_t decision = rh2 - (rw2 * rh) + (rw2 / 4);

  while ((twoRh2 * x) < (twoRw2 * y)) {
    x++;
    if (decision < 0) {
      decision += rh2 + (twoRh2 * x);
    } else {
      decision += rh2 + (twoRh2 * x) - (twoRw2 * y);
      drawFastHLine(x0 - (x - 1), y0 + y, 2 * (x - 1) + 1, color);
      drawFastHLine(x0 - (x - 1), y0 - y, 2 * (x - 1) + 1, color);
      y--;
    }
  }

  decision = ((rh2 * (2 * x + 1) * (2 * x + 1)) >> 2) + (rw2 * (y - 1) * (y - 1)) - (rw2 * rh2);
  while (y >= 0) {
    drawFastHLine(x0 - x, y0 + y, 2 * x + 1, color);
    drawFastHLine(x0 - x, y0 - y, 2 * x + 1, color);
    y--;
    if (decision > 0) {
      decision += rw2 - (twoRw2 * y);
    } else {
      decision += rw2 + (twoRh2 * x) - (twoRw2 * y);
      x++;
    }
  }
}

void BitmapPanel::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                                uint16_t color) {
  const int16_t maxRadius = ((w < h) ? w : h) / 2;
  if (r > maxRadius) r = maxRadius;
  writeFastHLine(x + r, y, w - 2 * r, color);
  writeFastHLine(x + r, y + h - 1, w - 2 * r, color);
  writeFastVLine(x, y + r, h - 2 * r, color);
  writeFastVLine(x + w - 1, y + r, h - 2 * r, color);
  drawCircleHelper(x + r, y + r, r, 1, color);
  drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
  drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
  drawCircleHelper(x + r, y + h - r - 1, r, 8, color);
}

void BitmapPanel::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                                uint16_t color) {
  const int16_t maxRadius = ((w < h) ? w : h) / 2;
  if (r > maxRadius) r = maxRadius;
  writeFillRect(x + r, y, w - 2 * r, h, color);
  fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
  fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

void BitmapPanel::rotatePoint(int16_t* x, int16_t* y, int16_t angleDegrees) {
  const float angleRad = (float)angleDegrees * kPi / 180.0f;
  const float s = sinf(angleRad);
  const float c = cosf(angleRad);
  const float originalX = (float)*x;
  const float originalY = (float)*y;
  *x = (int16_t)((originalX * c) - (originalY * s));
  *y = (int16_t)((originalX * s) + (originalY * c));
}

void BitmapPanel::drawRotatedRect(int16_t cenX, int16_t cenY, int16_t w, int16_t h,
                                  int16_t angle, uint16_t color) {
  if (w < 1 || h < 1) return;

  const int16_t W = w - 1;
  const int16_t H = h - 1;
  const int16_t halfW = W / 2;
  const int16_t halfH = H / 2;

  int16_t x0 = W - halfW, y0 = H - halfH;
  int16_t x1 = -halfW, y1 = H - halfH;
  int16_t x2 = -halfW, y2 = -halfH;
  int16_t x3 = W - halfW, y3 = -halfH;

  rotatePoint(&x0, &y0, angle);
  rotatePoint(&x1, &y1, angle);
  rotatePoint(&x2, &y2, angle);
  rotatePoint(&x3, &y3, angle);

  x0 += cenX;
  x1 += cenX;
  x2 += cenX;
  x3 += cenX;
  y0 += cenY;
  y1 += cenY;
  y2 += cenY;
  y3 += cenY;

  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x3, y3, color);
  drawLine(x3, y3, x0, y0, color);
}

void BitmapPanel::fillRotatedRect(int16_t cenX, int16_t cenY, int16_t w, int16_t h,
                                  int16_t angle, uint16_t color) {
  if (w < 1 || h < 1) return;

  const int16_t W = w - 1;
  const int16_t H = h - 1;
  const int16_t halfW = W / 2;
  const int16_t halfH = H / 2;

  int16_t x0 = W - halfW, y0 = H - halfH;
  int16_t x1 = -halfW, y1 = H - halfH;
  int16_t x2 = -halfW, y2 = -halfH;
  int16_t x3 = W - halfW, y3 = -halfH;

  rotatePoint(&x0, &y0, angle);
  rotatePoint(&x1, &y1, angle);
  rotatePoint(&x2, &y2, angle);
  rotatePoint(&x3, &y3, angle);

  x0 += cenX;
  x1 += cenX;
  x2 += cenX;
  x3 += cenX;
  y0 += cenY;
  y1 += cenY;
  y2 += cenY;
  y3 += cenY;

  drawTriangle(x0, y0, x1, y1, x2, y2, color);
  drawTriangle(x2, y2, x3, y3, x0, y0, color);
}

void BitmapPanel::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                               int16_t y2, uint16_t color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

void BitmapPanel::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                               int16_t y2, uint16_t color) {
  int16_t a, b, y, last;

  if (y0 > y1) {
    int16_t swap = y0;
    y0 = y1;
    y1 = swap;
    swap = x0;
    x0 = x1;
    x1 = swap;
  }
  if (y1 > y2) {
    int16_t swap = y2;
    y2 = y1;
    y1 = swap;
    swap = x2;
    x2 = x1;
    x1 = swap;
  }
  if (y0 > y1) {
    int16_t swap = y0;
    y0 = y1;
    y1 = swap;
    swap = x0;
    x0 = x1;
    x1 = swap;
  }

  if (y0 == y2) {
    a = b = x0;
    if (x1 < a) {
      a = x1;
    } else if (x1 > b) {
      b = x1;
    }
    if (x2 < a) {
      a = x2;
    } else if (x2 > b) {
      b = x2;
    }
    writeFastHLine(a, y0, b - a + 1, color);
    return;
  }

  const int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0, dx12 = x2 - x1,
                dy12 = y2 - y1;
  int32_t sa = 0, sb = 0;

  last = (y1 == y2) ? y1 : (int16_t)(y1 - 1);

  for (y = y0; y <= last; y++) {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    if (a > b) {
      const int16_t swap = a;
      a = b;
      b = swap;
    }
    writeFastHLine(a, y, b - a + 1, color);
  }

  sa = (int32_t)dx12 * (y - y1);
  sb = (int32_t)dx02 * (y - y0);
  for (; y <= y2; y++) {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    if (a > b) {
      const int16_t swap = a;
      a = b;
      b = swap;
    }
    writeFastHLine(a, y, b - a + 1, color);
  }
}

// --- text, the way Adafruit_GFX draws and measures it ----------------------

void BitmapPanel::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color,
                           uint16_t background, uint8_t size) {
  if (font_ == nullptr) {
    if ((x >= width_) || (y >= height_) || ((x + 6 * size - 1) < 0) || ((y + 8 * size - 1) < 0)) {
      return;
    }

    if (c >= 176) c++;  // the classic charset's high half, as Adafruit_GFX shifts it

    for (int8_t i = 0; i < 5; i++) {
      uint8_t line = font[c * 5 + i];
      for (int8_t j = 0; j < 8; j++, line >>= 1) {
        if (line & 1) {
          if (size == 1) {
            writePixel(x + i, y + j, color);
          } else {
            writeFillRect(x + i * size, y + j * size, size, size, color);
          }
        } else if (background != color) {
          if (size == 1) {
            writePixel(x + i, y + j, background);
          } else {
            writeFillRect(x + i * size, y + j * size, size, size, background);
          }
        }
      }
    }
    if (background != color) {
      if (size == 1) {
        writeFastVLine(x + 5, y, 8, background);
      } else {
        writeFillRect(x + 5 * size, y, size, 8 * size, background);
      }
    }
    return;
  }

  c -= (uint8_t)font_->first;
  const GFXglyph* glyph = &font_->glyph[c];
  const uint8_t* bitmap = font_->bitmap;

  uint16_t offset = glyph->bitmapOffset;
  const uint8_t w = glyph->width, h = glyph->height;
  const int8_t xo = glyph->xOffset, yo = glyph->yOffset;
  uint8_t bits = 0, bit = 0;

  for (uint8_t yy = 0; yy < h; yy++) {
    for (uint8_t xx = 0; xx < w; xx++) {
      if (!(bit++ & 7)) bits = bitmap[offset++];
      if (bits & 0x80) {
        if (size == 1) {
          writePixel(x + xo + xx, y + yo + yy, color);
        } else {
          writeFillRect(x + (xo + xx) * size, y + (yo + yy) * size, size, size, color);
        }
      }
      bits <<= 1;
    }
  }
}

void BitmapPanel::writeChar(uint8_t c) {
  if (font_ == nullptr) {
    if (c == '\n') {
      cursorX_ = 0;
      cursorY_ += textSize_ * 8;
    } else if (c != '\r') {
      if ((cursorX_ + textSize_ * 6) > width_) {
        cursorX_ = 0;
        cursorY_ += textSize_ * 8;
      }
      drawChar(cursorX_, cursorY_, c, textColor_, textBackground_, textSize_);
      cursorX_ += textSize_ * 6;
    }
    return;
  }

  if (c == '\n') {
    cursorX_ = 0;
    cursorY_ += (int16_t)textSize_ * (uint8_t)font_->yAdvance;
  } else if (c != '\r') {
    const uint8_t first = font_->first;
    if (c >= first && c <= font_->last) {
      const GFXglyph* glyph = &font_->glyph[c - first];
      const uint8_t w = glyph->width, h = glyph->height;
      if (w > 0 && h > 0) {
        const int16_t xo = (int8_t)glyph->xOffset;
        if ((cursorX_ + textSize_ * (xo + w)) > width_) {
          cursorX_ = 0;
          cursorY_ += (int16_t)textSize_ * (uint8_t)font_->yAdvance;
        }
        drawChar(cursorX_, cursorY_, c, textColor_, textBackground_, textSize_);
      }
      cursorX_ += (uint8_t)glyph->xAdvance * (int16_t)textSize_;
    }
  }
}

size_t BitmapPanel::print(const char* text) {
  size_t written = 0;
  while (*text) {
    writeChar((uint8_t)*text++);
    written++;
  }
  return written;
}

void BitmapPanel::charBounds(unsigned char c, int16_t* x, int16_t* y, int16_t* minX,
                             int16_t* minY, int16_t* maxX, int16_t* maxY) {
  if (font_ != nullptr) {
    if (c == '\n') {
      *x = 0;
      *y += textSize_ * (uint8_t)font_->yAdvance;
    } else if (c != '\r') {
      const uint8_t first = font_->first, last = font_->last;
      if (c >= first && c <= last) {
        const GFXglyph* glyph = &font_->glyph[c - first];
        const uint8_t gw = glyph->width, gh = glyph->height, xa = glyph->xAdvance;
        const int8_t xo = glyph->xOffset, yo = glyph->yOffset;
        if ((*x + (((int16_t)xo + gw) * textSize_)) > width_) {
          *x = 0;
          *y += (int16_t)textSize_ * (uint8_t)font_->yAdvance;
        }
        const int16_t ts = (int16_t)textSize_;
        const int16_t x1 = *x + xo * ts, y1 = *y + yo * ts;
        const int16_t x2 = x1 + gw * ts - 1, y2 = y1 + gh * ts - 1;
        if (x1 < *minX) *minX = x1;
        if (y1 < *minY) *minY = y1;
        if (x2 > *maxX) *maxX = x2;
        if (y2 > *maxY) *maxY = y2;
        *x += xa * ts;
      }
    }
    return;
  }

  if (c == '\n') {
    *x = 0;
    *y += textSize_ * 8;
  } else if (c != '\r') {
    if ((*x + textSize_ * 6) > width_) {
      *x = 0;
      *y += textSize_ * 8;
    }
    const int16_t x2 = *x + textSize_ * 6 - 1, y2 = *y + textSize_ * 8 - 1;
    if (x2 > *maxX) *maxX = x2;
    if (y2 > *maxY) *maxY = y2;
    if (*x < *minX) *minX = *x;
    if (*y < *minY) *minY = *y;
    *x += textSize_ * 6;
  }
}

void BitmapPanel::getTextBounds(const char* text, int16_t x, int16_t y, int16_t* outX,
                                int16_t* outY, uint16_t* outWidth, uint16_t* outHeight) {
  int16_t cursorX = x;
  int16_t cursorY = y;
  int16_t minX = 0x7FFF, minY = 0x7FFF, maxX = -1, maxY = -1;

  while (*text) charBounds((unsigned char)*text++, &cursorX, &cursorY, &minX, &minY, &maxX, &maxY);

  if (outX != nullptr) *outX = minX;
  if (outY != nullptr) *outY = minY;
  if (outWidth != nullptr) *outWidth = (maxX >= minX) ? (uint16_t)(maxX - minX + 1) : 0;
  if (outHeight != nullptr) *outHeight = (maxY >= minY) ? (uint16_t)(maxY - minY + 1) : 0;
}

// --- the BMP ---------------------------------------------------------------

namespace {

void put16(FILE* file, uint16_t value) {
  fputc(value & 0xFF, file);
  fputc((value >> 8) & 0xFF, file);
}

void put32(FILE* file, uint32_t value) {
  fputc(value & 0xFF, file);
  fputc((value >> 8) & 0xFF, file);
  fputc((value >> 16) & 0xFF, file);
  fputc((value >> 24) & 0xFF, file);
}

}  // namespace

const uint8_t* BitmapPanel::classicFont() { return font; }

// The frame one bit per pixel, row-major and top-down (GIF order, unlike save()'s bottom-up BMP).
void BitmapPanel::packFrame(uint8_t* out) const {
  const int rowBytes = width_ / 8;
  for (int y = 0; y < height_; y++) {
    for (int byte = 0; byte < rowBytes; byte++) {
      uint8_t bits = 0;
      for (int bit = 0; bit < 8; bit++) {
        bits = (uint8_t)(bits << 1);
        const uint8_t pixel = (uint8_t)(frame_[y][byte * 8 + bit] & 1);
        if ((pixel ^ (inverted_ ? 1 : 0)) != 0) bits |= 1;
      }
      out[y * rowBytes + byte] = bits;
    }
  }
}

// A one-bit BMP, rows bottom up: the panel's own depth, and the format scripts/asset.py reads.
bool BitmapPanel::save(const char* path) const {
  const int rowBytes = width_ / 8;
  const int imageBytes = rowBytes * height_;
  const int headerBytes = 14 + 40;

  FILE* file = fopen(path, "wb");
  if (file == nullptr) return false;

  fputc('B', file);
  fputc('M', file);
  put32(file, (uint32_t)(headerBytes + 8 + imageBytes));  // file size
  put16(file, 0);
  put16(file, 0);
  put32(file, (uint32_t)(headerBytes + 8));  // where the pixels start

  put32(file, 40);  // this header's size
  put32(file, (uint32_t)width_);
  put32(file, (uint32_t)height_);
  put16(file, 1);  // planes
  put16(file, 1);  // bits per pixel
  put32(file, 0);  // no compression
  put32(file, (uint32_t)imageBytes);
  put32(file, 2835);  // 72 dpi, in pixels per metre
  put32(file, 2835);
  put32(file, 2);  // palette entries used
  put32(file, 2);

  // Index 0 paper, index 1 ink: BMP calls them B, G, R, reserved.
  fputc(0xFF, file);
  fputc(0xFF, file);
  fputc(0xFF, file);
  fputc(0x00, file);
  fputc(0x00, file);
  fputc(0x00, file);
  fputc(0x00, file);
  fputc(0x00, file);

  for (int y = height_ - 1; y >= 0; y--) {
    for (int byte = 0; byte < rowBytes; byte++) {
      uint8_t bits = 0;
      for (int bit = 0; bit < 8; bit++) {
        bits = (uint8_t)(bits << 1);
        const uint8_t pixel = (uint8_t)(frame_[y][byte * 8 + bit] & 1);
        if ((pixel ^ (inverted_ ? 1 : 0)) != 0) bits |= 1;
      }
      fputc(bits, file);
    }
  }

  return fclose(file) == 0;
}
