#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

// The panel the check draws on: a 128 x 64 one-bit frame behind the Adafruit_GFX verb set that
// AdafruitDisplay forwards to. The firmware's Display chain is unchanged above it — the real
// AdafruitDisplay — so a page that draws here takes the same path it takes on the device, and the
// only thing that differs is what is behind the last call. The status band above the page is the
// ux component's, drawn in Berry like the page itself.
//
// The primitives and the text path are the port frame.c held: they follow the installed Adafruit
// GFX closely enough that a page's face choice, its measured box and its pixels are the panel's.
// Colours are the drivers': 0 is paper, 1 is ink, and 2 flips whatever the pixel already is.
//
// packFrame() hands the frame to the harness's GIF writer; the panel itself never writes a file.

class BitmapPanel {
 public:
  static constexpr int16_t Width = 128;
  static constexpr int16_t Height = 64;

  explicit BitmapPanel(int16_t width = Width, int16_t height = Height);

  // --- the panel -----------------------------------------------------------
  bool begin();
  // The buffer is already the frame; nothing has to be pushed anywhere.
  void display() {}
  void invertDisplay(bool on) { inverted_ = on; }

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }
  uint8_t getRotation() const { return rotation_; }
  void setRotation(uint8_t rotation) { rotation_ = (uint8_t)(rotation & 3); }

  // --- pixels, lines and shapes --------------------------------------------
  void fillScreen(uint16_t color);
  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
  void drawEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color);
  void fillEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color);
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint16_t color);
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint16_t color);
  void drawRotatedRect(int16_t cenX, int16_t cenY, int16_t w, int16_t h, int16_t angle,
                       uint16_t color);
  void fillRotatedRect(int16_t cenX, int16_t cenY, int16_t w, int16_t h, int16_t angle,
                       uint16_t color);

  // --- text ----------------------------------------------------------------
  void setFont(const GFXfont* face) { font_ = face; }
  void setTextSize(uint8_t size) { textSize_ = size == 0 ? 1 : size; }
  void setTextColor(uint16_t color) { textColor_ = textBackground_ = color; }
  void setTextColor(uint16_t color, uint16_t background) {
    textColor_ = color;
    textBackground_ = background;
  }
  void setCursor(int16_t x, int16_t y) {
    cursorX_ = x;
    cursorY_ = y;
  }
  size_t print(const char* text);
  void getTextBounds(const char* text, int16_t x, int16_t y, int16_t* outX, int16_t* outY,
                     uint16_t* outWidth, uint16_t* outHeight);
  // The raw glyph draw the display wrapper's glyph verb needs, for codes print() treats as
  // control characters.
  void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t background,
                uint8_t size);

  // --- the picture ---------------------------------------------------------
  // The frame packed one bit per pixel, row-major and top-down, as the GIF writer wants it.
  void packFrame(uint8_t* out) const;
  // The classic 5x7 face this panel draws with, for host-side callouts that render into a packed
  // frame themselves.
  static const uint8_t* classicFont();

 private:
  void writePixel(int16_t x, int16_t y, uint16_t color);
  void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, uint16_t color);
  void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta,
                        uint16_t color);
  void rotatePoint(int16_t* x, int16_t* y, int16_t angleDegrees);
  void writeChar(uint8_t c);
  void charBounds(unsigned char c, int16_t* x, int16_t* y, int16_t* minX, int16_t* minY,
                  int16_t* maxX, int16_t* maxY);

  int16_t width_;
  int16_t height_;
  uint8_t rotation_ = 0;
  bool inverted_ = false;
  uint8_t frame_[Height][Width] = {};

  const GFXfont* font_ = nullptr;
  uint8_t textSize_ = 1;
  uint16_t textColor_ = 1;
  uint16_t textBackground_ = 1;
  int16_t cursorX_ = 0;
  int16_t cursorY_ = 0;
};
