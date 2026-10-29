#include "Text.h"

#include <Arduino.h>

#include "BuiltinFont.h"

// The glyph-writing rules in this file are taken from Adafruit_GFX 1.12.6's drawChar.
//
// Software License Agreement (BSD License)
//
// Copyright (c) 2012 Adafruit Industries.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// - Redistributions of source code must retain the above copyright notice, this list of conditions
//   and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright notice, this list of
//   conditions and the following disclaimer in the documentation and/or other materials provided
//   with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
// WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

namespace gardener::gfx::shapes {
namespace {

// A glyph pixel is a size-by-size block of the frame, so one pixel and one scale share the loop.
void drawBlock(Canvas& canvas, int16_t x, int16_t y, uint8_t size, uint8_t color) {
  for (uint8_t row = 0; row < size; row++) {
    for (uint8_t column = 0; column < size; column++) {
      canvas.setPixel(static_cast<int16_t>(x + column), static_cast<int16_t>(y + row), color);
    }
  }
}

// Draws one glyph and returns how far the cursor moves for it: zero when the font has no glyph
// for the character, and the glyph's own advance otherwise.
int16_t drawFontGlyph(Canvas& canvas, int16_t x, int16_t baseline, const Font* font,
                      uint8_t character, uint8_t size, uint8_t color) {
  if (character < font->first || character > font->last) return 0;
  const Glyph& glyph = font->glyph[character - font->first];
  const uint8_t width = pgm_read_byte(&glyph.width);
  const uint8_t height = pgm_read_byte(&glyph.height);
  const int16_t xOffset = static_cast<int8_t>(pgm_read_byte(&glyph.xOffset));
  const int16_t yOffset = static_cast<int8_t>(pgm_read_byte(&glyph.yOffset));
  uint16_t offset = pgm_read_word(&glyph.bitmapOffset);
  uint8_t bits = 0;
  uint8_t bit = 0;
  for (uint8_t row = 0; row < height; row++) {
    for (uint8_t column = 0; column < width; column++) {
      if ((bit++ & 7) == 0) bits = pgm_read_byte(&font->bitmap[offset++]);
      if ((bits & 0x80) != 0) {
        drawBlock(canvas, static_cast<int16_t>(x + (xOffset + column) * size),
                  static_cast<int16_t>(baseline + (yOffset + row) * size), size, color);
      }
      bits = static_cast<uint8_t>(bits << 1);
    }
  }
  return static_cast<int16_t>(pgm_read_byte(&glyph.xAdvance) * size);
}

}  // namespace

void Text::operator()(Canvas& canvas) const {
  if (size == 0) return;
  const Font* font = face != nullptr ? face : &BuiltinFont;
  int16_t cursorX = x;
  for (const char* character = value; *character != '\0'; character++) {
    cursorX = static_cast<int16_t>(
        cursorX + drawFontGlyph(canvas, cursorX, y, font, static_cast<uint8_t>(*character), size,
                                color));
  }
}

TextBox measureText(const Font* font, const char* value, uint8_t size) {
  if (value == nullptr || size == 0) return {0, 0};

  if (font == nullptr) {
    // The classic face measures as a run of six-by-eight cells, including the blank column behind
    // the glyph that the advance leaves.
    const int16_t cellWidth = static_cast<int16_t>(6 * size);
    const int16_t cellHeight = static_cast<int16_t>(8 * size);
    int16_t width = 0;
    for (const char* character = value; *character != '\0'; character++) {
      width = static_cast<int16_t>(width + cellWidth);
    }
    return {width, width == 0 ? static_cast<int16_t>(0) : cellHeight};
  }

  int16_t minX = 0x7FFF;
  int16_t minY = 0x7FFF;
  int16_t maxX = -1;
  int16_t maxY = -1;
  int16_t cursorX = 0;
  for (const char* character = value; *character != '\0'; character++) {
    const uint8_t code = static_cast<uint8_t>(*character);
    if (code < font->first || code > font->last) continue;
    const Glyph& glyph = font->glyph[code - font->first];
    if (glyph.width > 0 && glyph.height > 0) {
      const int16_t x1 = static_cast<int16_t>(cursorX + glyph.xOffset * size);
      const int16_t y1 = static_cast<int16_t>(glyph.yOffset * size);
      const int16_t x2 = static_cast<int16_t>(x1 + glyph.width * size - 1);
      const int16_t y2 = static_cast<int16_t>(y1 + glyph.height * size - 1);
      if (x1 < minX) minX = x1;
      if (y1 < minY) minY = y1;
      if (x2 > maxX) maxX = x2;
      if (y2 > maxY) maxY = y2;
    }
    cursorX = static_cast<int16_t>(cursorX + glyph.xAdvance * size);
  }
  if (maxX < minX || maxY < minY) return {0, 0};
  return {static_cast<int16_t>(maxX - minX + 1), static_cast<int16_t>(maxY - minY + 1)};
}

void drawGlyph(Canvas& canvas, int16_t x, int16_t y, uint8_t code, uint8_t size, uint8_t color) {
  if (size == 0) return;
  const int16_t baseline = static_cast<int16_t>(y + 7 * size);
  drawFontGlyph(canvas, x, baseline, &BuiltinFont, code, size, color);
}

}  // namespace gardener::gfx::shapes
