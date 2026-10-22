// frame.c — the host's panel for the Berry check.
//
// The harness's screen stub forwards every drawing verb here, and this file holds the pixels: a
// 128 by 64 one-bit frame with the primitives the panel's driver is handed. Every one of them is a
// port of the Adafruit_GFX the device compiles against, and so is the text path — the classic 5x7
// from glcdfont.c, the FreeSans and FreeMono faces from Fonts/, drawChar, write and
// getTextBounds — so a page's face choice and its measured text box are the panel's.
//
// The font data is the installed library's, not a copy: check.py puts the Adafruit GFX library on
// the include path, and gfx/Adafruit_GFX.h supplies the structs its font headers expect.
//
// save() writes a 1-bit BMP: the format every viewer opens and scripts/asset.py reads. The overlay's
// twelve-row band is marked on the way out with a one-pixel checker, so a preview shows where it
// covers the page.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "berry.h"
#include "frame.h"

#include <glcdfont.c>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMono24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/Picopixel.h>
#include <Fonts/TomThumb.h>

enum {
  FrameWidth = 128,
  FrameHeight = 64,
  BandY = 52,  // the overlay's top row: its band covers the twelve rows below this
  Ink = 1,
  Paper = 0,
};

// The faces a page may name, in the order Fonts.cpp lists them.
typedef struct {
  const char* name;
  const GFXfont* face;
} NamedFace;

static const NamedFace kFaces[] = {
    {"sans9", &FreeSans9pt7b},
    {"sans12", &FreeSans12pt7b},
    {"sans18", &FreeSans18pt7b},
    {"sans24", &FreeSans24pt7b},
    {"sansbold9", &FreeSansBold9pt7b},
    {"sansbold12", &FreeSansBold12pt7b},
    {"sansbold18", &FreeSansBold18pt7b},
    {"sansbold24", &FreeSansBold24pt7b},
    {"mono9", &FreeMono9pt7b},
    {"mono12", &FreeMono12pt7b},
    {"mono18", &FreeMono18pt7b},
    {"mono24", &FreeMono24pt7b},
    {"monobold9", &FreeMonoBold9pt7b},
    {"monobold12", &FreeMonoBold12pt7b},
    {"monobold18", &FreeMonoBold18pt7b},
    {"monobold24", &FreeMonoBold24pt7b},
    {"tomthumb", &TomThumb},
    {"picopixel", &Picopixel},
};

static uint8_t sFrame[FrameHeight][FrameWidth];

// The text state Adafruit_GFX keeps: the face, the scale, the two colours and the cursor.
static const GFXfont* sFont = NULL;
static uint8_t sTextSize = 1;
static uint16_t sTextColor = Ink;
static uint16_t sTextBackground = Ink;
static int16_t sCursorX = 0;
static int16_t sCursorY = 0;

static const GFXfont* findFace(const char* name) {
  if (name == NULL || strcmp(name, "builtin") == 0) return NULL;
  for (size_t i = 0; i < sizeof(kFaces) / sizeof(kFaces[0]); i++) {
    if (strcmp(name, kFaces[i].name) == 0) return kFaces[i].face;
  }
  return (const GFXfont*)1;  // not a face: callers tell this from NULL (the built-in)
}

// --- pixels, the way Adafruit_GFX puts them down ---------------------------

static void writePixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= FrameWidth || y >= FrameHeight) return;
  sFrame[y][x] = (uint8_t)(color & 1);
}

static void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
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

static void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  writeLine(x, y, x + w - 1, y, color);
}

static void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  writeLine(x, y, x, y + h - 1, color);
}

static void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t i = x; i < x + w; i++) writeFastVLine(i, y, h, color);
}

static void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
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

static void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  writeLine(x, y, x + w - 1, y, color);
}

static void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  writeLine(x, y, x, y + h - 1, color);
}

static void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t i = x; i < x + w; i++) writeFastVLine(i, y, h, color);
}

static void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  writeFastHLine(x, y, w, color);
  writeFastHLine(x, y + h - 1, w, color);
  writeFastVLine(x, y, h, color);
  writeFastVLine(x + w - 1, y, h, color);
}

static void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners,
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

static void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
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

static void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners,
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

static void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  writeFastVLine(x0, y0 - r, 2 * r + 1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

static void drawEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color) {
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

  decision = ((rh2 * (2 * x + 1) * (2 * x + 1)) >> 2) + (rw2 * (y - 1) * (y - 1)) -
             (rw2 * rh2);
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

static void fillEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color) {
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

  decision = ((rh2 * (2 * x + 1) * (2 * x + 1)) >> 2) + (rw2 * (y - 1) * (y - 1)) -
             (rw2 * rh2);
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

static void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
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

static void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                          uint16_t color) {
  const int16_t maxRadius = ((w < h) ? w : h) / 2;
  if (r > maxRadius) r = maxRadius;
  writeFillRect(x + r, y, w - 2 * r, h, color);
  fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
  fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

// The rotated rectangles are two triangles, and the triangles are below them.
static void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                         int16_t y2, uint16_t color);
static void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                         int16_t y2, uint16_t color);

static void rotatePoint(int16_t* x0, int16_t* y0, int16_t angleDeg) {
  const float angleRad = (float)angleDeg * (float)M_PI / 180.0f;
  const float s = sinf(angleRad);
  const float c = cosf(angleRad);
  const float x = (float)*x0;
  const float y = (float)*y0;
  *x0 = (int16_t)((x * c) - (y * s));
  *y0 = (int16_t)((x * s) + (y * c));
}

static void drawRotatedRect(int16_t cenX, int16_t cenY, int16_t w, int16_t h,
                            int16_t angleDeg, uint16_t color) {
  if (w < 1 || h < 1) return;

  const int16_t W = w - 1;
  const int16_t H = h - 1;
  const int16_t halfW = W / 2;
  const int16_t halfH = H / 2;

  int16_t x0 = W - halfW, y0 = H - halfH;
  int16_t x1 = -halfW, y1 = H - halfH;
  int16_t x2 = -halfW, y2 = -halfH;
  int16_t x3 = W - halfW, y3 = -halfH;

  rotatePoint(&x0, &y0, angleDeg);
  rotatePoint(&x1, &y1, angleDeg);
  rotatePoint(&x2, &y2, angleDeg);
  rotatePoint(&x3, &y3, angleDeg);

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

static void fillRotatedRect(int16_t cenX, int16_t cenY, int16_t w, int16_t h,
                            int16_t angleDeg, uint16_t color) {
  if (w < 1 || h < 1) return;

  const int16_t W = w - 1;
  const int16_t H = h - 1;
  const int16_t halfW = W / 2;
  const int16_t halfH = H / 2;

  int16_t x0 = W - halfW, y0 = H - halfH;
  int16_t x1 = -halfW, y1 = H - halfH;
  int16_t x2 = -halfW, y2 = -halfH;
  int16_t x3 = W - halfW, y3 = -halfH;

  rotatePoint(&x0, &y0, angleDeg);
  rotatePoint(&x1, &y1, angleDeg);
  rotatePoint(&x2, &y2, angleDeg);
  rotatePoint(&x3, &y3, angleDeg);

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

static void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                         int16_t y2, uint16_t color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

static void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
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

  const int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
                dx12 = x2 - x1, dy12 = y2 - y1;
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

static void setFont(const GFXfont* face) { sFont = face; }

static void setTextSize(uint8_t size) { sTextSize = size; }

static void setTextColor(uint16_t color) {
  sTextColor = color;
  sTextBackground = color;
}

static void setTextColor2(uint16_t color, uint16_t background) {
  sTextColor = color;
  sTextBackground = background;
}

static void setCursor(int16_t x, int16_t y) {
  sCursorX = x;
  sCursorY = y;
}

static void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg,
                     uint8_t size) {
  if (!sFont) {
    if ((x >= FrameWidth) || (y >= FrameHeight) || ((x + 6 * size - 1) < 0) ||
        ((y + 8 * size - 1) < 0)) {
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
        } else if (bg != color) {
          if (size == 1) {
            writePixel(x + i, y + j, bg);
          } else {
            writeFillRect(x + i * size, y + j * size, size, size, bg);
          }
        }
      }
    }
    if (bg != color) {
      if (size == 1) {
        writeFastVLine(x + 5, y, 8, bg);
      } else {
        writeFillRect(x + 5 * size, y, size, 8 * size, bg);
      }
    }
    return;
  }

  c -= (uint8_t)sFont->first;
  const GFXglyph* glyph = &sFont->glyph[c];
  const uint8_t* bitmap = sFont->bitmap;

  uint16_t bo = glyph->bitmapOffset;
  const uint8_t w = glyph->width, h = glyph->height;
  const int8_t xo = glyph->xOffset, yo = glyph->yOffset;
  uint8_t bits = 0, bit = 0;

  for (uint8_t yy = 0; yy < h; yy++) {
    for (uint8_t xx = 0; xx < w; xx++) {
      if (!(bit++ & 7)) bits = bitmap[bo++];
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

static void writeChar(uint8_t c) {
  if (!sFont) {
    if (c == '\n') {
      sCursorX = 0;
      sCursorY += sTextSize * 8;
    } else if (c != '\r') {
      if ((sCursorX + sTextSize * 6) > FrameWidth) {
        sCursorX = 0;
        sCursorY += sTextSize * 8;
      }
      drawChar(sCursorX, sCursorY, c, sTextColor, sTextBackground, sTextSize);
      sCursorX += sTextSize * 6;
    }
    return;
  }

  if (c == '\n') {
    sCursorX = 0;
    sCursorY += (int16_t)sTextSize * (uint8_t)sFont->yAdvance;
  } else if (c != '\r') {
    const uint8_t first = sFont->first;
    if (c >= first && c <= sFont->last) {
      const GFXglyph* glyph = &sFont->glyph[c - first];
      const uint8_t w = glyph->width, h = glyph->height;
      if (w > 0 && h > 0) {
        const int16_t xo = (int8_t)glyph->xOffset;
        if ((sCursorX + sTextSize * (xo + w)) > FrameWidth) {
          sCursorX = 0;
          sCursorY += (int16_t)sTextSize * (uint8_t)sFont->yAdvance;
        }
        drawChar(sCursorX, sCursorY, c, sTextColor, sTextBackground, sTextSize);
      }
      sCursorX += (uint8_t)glyph->xAdvance * (int16_t)sTextSize;
    }
  }
}

static void printText(const char* text) {
  while (*text) writeChar((uint8_t)*text++);
}

static void charBounds(unsigned char c, int16_t* x, int16_t* y, int16_t* minx,
                       int16_t* miny, int16_t* maxx, int16_t* maxy) {
  if (sFont) {
    if (c == '\n') {
      *x = 0;
      *y += sTextSize * (uint8_t)sFont->yAdvance;
    } else if (c != '\r') {
      const uint8_t first = sFont->first, last = sFont->last;
      if (c >= first && c <= last) {
        const GFXglyph* glyph = &sFont->glyph[c - first];
        const uint8_t gw = glyph->width, gh = glyph->height, xa = glyph->xAdvance;
        const int8_t xo = glyph->xOffset, yo = glyph->yOffset;
        if ((*x + (((int16_t)xo + gw) * sTextSize)) > FrameWidth) {
          *x = 0;
          *y += (int16_t)sTextSize * (uint8_t)sFont->yAdvance;
        }
        const int16_t ts = (int16_t)sTextSize;
        const int16_t x1 = *x + xo * ts, y1 = *y + yo * ts;
        const int16_t x2 = x1 + gw * ts - 1, y2 = y1 + gh * ts - 1;
        if (x1 < *minx) *minx = x1;
        if (y1 < *miny) *miny = y1;
        if (x2 > *maxx) *maxx = x2;
        if (y2 > *maxy) *maxy = y2;
        *x += xa * ts;
      }
    }
    return;
  }

  if (c == '\n') {
    *x = 0;
    *y += sTextSize * 8;
  } else if (c != '\r') {
    if ((*x + sTextSize * 6) > FrameWidth) {
      *x = 0;
      *y += sTextSize * 8;
    }
    const int16_t x2 = *x + sTextSize * 6 - 1, y2 = *y + sTextSize * 8 - 1;
    if (x2 > *maxx) *maxx = x2;
    if (y2 > *maxy) *maxy = y2;
    if (*x < *minx) *minx = *x;
    if (*y < *miny) *miny = *y;
    *x += sTextSize * 6;
  }
}

static void textBounds(const char* text, int16_t* outWidth, int16_t* outHeight) {
  int16_t x = 0, y = 0;
  int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;

  while (*text) charBounds((unsigned char)*text++, &x, &y, &minx, &miny, &maxx, &maxy);

  *outWidth = (maxx >= minx) ? (int16_t)(maxx - minx + 1) : 0;
  *outHeight = (maxy >= miny) ? (int16_t)(maxy - miny + 1) : 0;
}

// --- the BMP ---------------------------------------------------------------

static void put16(FILE* file, uint16_t value) {
  fputc(value & 0xFF, file);
  fputc((value >> 8) & 0xFF, file);
}

static void put32(FILE* file, uint32_t value) {
  fputc(value & 0xFF, file);
  fputc((value >> 8) & 0xFF, file);
  fputc((value >> 16) & 0xFF, file);
  fputc((value >> 24) & 0xFF, file);
}

// The overlay is a 12 px opaque band across the bottom, drawn in Overlay.cpp over whatever a page
// put there. The frame paints the same rows the same way, with a one-pixel checker: opaque, like
// the band, and a grey that reads as covered rather than as a page's own ink.
static void drawBand(void) {
  for (int16_t y = BandY; y < FrameHeight; y++) {
    for (int16_t x = 0; x < FrameWidth; x++) {
      sFrame[y][x] = ((x + y) % 2 == 0) ? Ink : Paper;
    }
  }
}

// A one-bit BMP, rows bottom up: the panel's own depth, and the format scripts/asset.py reads.
static int saveBmp(const char* path) {
  const int rowBytes = FrameWidth / 8;
  const int imageBytes = rowBytes * FrameHeight;
  const int headerBytes = 14 + 40;

  FILE* file = fopen(path, "wb");
  if (file == NULL) return 0;

  fputc('B', file);
  fputc('M', file);
  put32(file, (uint32_t)(headerBytes + 8 + imageBytes));  // file size
  put16(file, 0);
  put16(file, 0);
  put32(file, (uint32_t)(headerBytes + 8));  // where the pixels start

  put32(file, 40);  // this header's size
  put32(file, FrameWidth);
  put32(file, FrameHeight);
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

  for (int y = FrameHeight - 1; y >= 0; y--) {
    for (int byte = 0; byte < rowBytes; byte++) {
      uint8_t bits = 0;
      for (int bit = 0; bit < 8; bit++) {
        bits = (uint8_t)(bits << 1);
        if (sFrame[y][byte * 8 + bit] == Ink) bits |= 1;
      }
      fputc(bits, file);
    }
  }

  return fclose(file) == 0;
}

// --- the Berry module ------------------------------------------------------

static int argInt(bvm* vm, int index) { return (int)be_toint(vm, index); }

static const GFXfont* argFace(bvm* vm, int index, const char* where) {
  if (be_top(vm) < index || be_isnil(vm, index)) return NULL;
  const char* name = be_tostring(vm, index);
  const GFXfont* face = findFace(name);
  if (face == (const GFXfont*)1) {
    char message[96];
    snprintf(message, sizeof(message), "%s: unknown font %s", where, name);
    be_raise(vm, "value_error", message);
  }
  return face;
}

static int frameClear(bvm* vm) {
  memset(sFrame, Paper, sizeof(sFrame));
  be_return_nil(vm);
}

static int framePixel(bvm* vm) {
  writePixel((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), Ink);
  be_return_nil(vm);
}

static int frameLine(bvm* vm) {
  drawLine((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
           (int16_t)argInt(vm, 4), Ink);
  be_return_nil(vm);
}

static int frameHLine(bvm* vm) {
  drawFastHLine((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3), Ink);
  be_return_nil(vm);
}

static int frameVLine(bvm* vm) {
  drawFastVLine((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3), Ink);
  be_return_nil(vm);
}

static int frameRect(bvm* vm) {
  drawRect((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
           (int16_t)argInt(vm, 4), Ink);
  be_return_nil(vm);
}

static int frameFillRect(bvm* vm) {
  fillRect((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
           (int16_t)argInt(vm, 4), Ink);
  be_return_nil(vm);
}

static int frameRoundRect(bvm* vm) {
  drawRoundRect((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
                (int16_t)argInt(vm, 4), (int16_t)argInt(vm, 5), Ink);
  be_return_nil(vm);
}

static int frameFillRoundRect(bvm* vm) {
  fillRoundRect((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
                (int16_t)argInt(vm, 4), (int16_t)argInt(vm, 5), Ink);
  be_return_nil(vm);
}

static int frameCircle(bvm* vm) {
  drawCircle((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3), Ink);
  be_return_nil(vm);
}

static int frameFillCircle(bvm* vm) {
  fillCircle((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3), Ink);
  be_return_nil(vm);
}

static int frameEllipse(bvm* vm) {
  drawEllipse((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
              (int16_t)argInt(vm, 4), Ink);
  be_return_nil(vm);
}

static int frameFillEllipse(bvm* vm) {
  fillEllipse((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
              (int16_t)argInt(vm, 4), Ink);
  be_return_nil(vm);
}

static int frameTriangle(bvm* vm) {
  drawTriangle((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
               (int16_t)argInt(vm, 4), (int16_t)argInt(vm, 5), (int16_t)argInt(vm, 6), Ink);
  be_return_nil(vm);
}

static int frameFillTriangle(bvm* vm) {
  fillTriangle((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
               (int16_t)argInt(vm, 4), (int16_t)argInt(vm, 5), (int16_t)argInt(vm, 6), Ink);
  be_return_nil(vm);
}

static int frameRotatedRect(bvm* vm) {
  drawRotatedRect((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
                  (int16_t)argInt(vm, 4), (int16_t)argInt(vm, 5), Ink);
  be_return_nil(vm);
}

static int frameFillRotatedRect(bvm* vm) {
  fillRotatedRect((int16_t)argInt(vm, 1), (int16_t)argInt(vm, 2), (int16_t)argInt(vm, 3),
                  (int16_t)argInt(vm, 4), (int16_t)argInt(vm, 5), Ink);
  be_return_nil(vm);
}

// The device's screen.draw_text sets the face, the size and the colour and then prints at the
// cursor; the inverted flag is the paper-on-ink background the driver asks for.
static int frameText(bvm* vm) {
  if (be_top(vm) < 6) be_raise(vm, "type_error", "frame.text(x, y, size, inverted, text, font)");
  const int16_t x = (int16_t)argInt(vm, 1);
  const int16_t y = (int16_t)argInt(vm, 2);
  const uint8_t size = (uint8_t)argInt(vm, 3);
  const int inverted = be_tobool(vm, 4);
  const char* text = be_tostring(vm, 5);

  setFont(argFace(vm, 6, "frame.text"));
  setTextSize(size);
  if (inverted) {
    setTextColor2(Paper, Ink);
  } else {
    setTextColor(Ink);
  }
  setCursor(x, y);
  printText(text);
  be_return_nil(vm);
}

// The ink box of a string, as Display::textBounds answers it.
static int frameBounds(bvm* vm) {
  if (be_top(vm) < 3) be_raise(vm, "type_error", "frame.bounds(text, size, font)");
  const char* text = be_tostring(vm, 1);
  const uint8_t size = (uint8_t)argInt(vm, 2);

  setFont(argFace(vm, 3, "frame.bounds"));
  setTextSize(size);

  int16_t width = 0, height = 0;
  textBounds(text, &width, &height);

  be_newobject(vm, "list");
  const int list = be_absindex(vm, -1);
  be_pushint(vm, width);
  be_data_push(vm, list);
  be_pop(vm, 1);
  be_pushint(vm, height);
  be_data_push(vm, list);
  be_pop(vm, 1);
  be_pop(vm, 1);
  be_return(vm);
}

static int frameKnown(bvm* vm) {
  if (be_top(vm) < 1 || be_isnil(vm, 1)) {
    be_pushbool(vm, 1);
    be_return(vm);
  }
  const char* name = be_tostring(vm, 1);
  be_pushbool(vm, findFace(name) != (const GFXfont*)1);
  be_return(vm);
}

static int frameSave(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", "frame.save(path)");
  }
  drawBand();
  if (!saveBmp(be_tostring(vm, 1))) be_raise(vm, "io_error", "frame.save: cannot write the file");
  be_return_nil(vm);
}

static const struct {
  const char* name;
  bntvfunc function;
} kFrameLibrary[] = {
    {"clear", frameClear},
    {"pixel", framePixel},
    {"line", frameLine},
    {"hline", frameHLine},
    {"vline", frameVLine},
    {"rect", frameRect},
    {"fillrect", frameFillRect},
    {"roundrect", frameRoundRect},
    {"fillroundrect", frameFillRoundRect},
    {"circle", frameCircle},
    {"fillcircle", frameFillCircle},
    {"ellipse", frameEllipse},
    {"fillellipse", frameFillEllipse},
    {"triangle", frameTriangle},
    {"filltriangle", frameFillTriangle},
    {"rotatedrect", frameRotatedRect},
    {"fillrotatedrect", frameFillRotatedRect},
    {"text", frameText},
    {"bounds", frameBounds},
    {"known", frameKnown},
    {"save", frameSave},
};

void frameRegister(bvm* vm) {
  be_newmodule(vm);
  const int module = be_absindex(vm, -1);
  for (size_t i = 0; i < sizeof(kFrameLibrary) / sizeof(kFrameLibrary[0]); i++) {
    be_pushntvfunction(vm, kFrameLibrary[i].function);
    be_setmember(vm, module, kFrameLibrary[i].name);
    be_pop(vm, 1);
  }
  be_setglobal(vm, "frame");
  be_pop(vm, 1);
}
