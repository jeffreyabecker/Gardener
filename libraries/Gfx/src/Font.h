#pragma once

// The font structures a Text draws through. They are forked from Adafruit_GFX 1.12.6's gfxfont.h
// into the Gfx namespace, with the fixed-metric fields at the end added by this library.
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

#include <stdint.h>

namespace gardener::gfx {

// One glyph of a variable face. `bitmapOffset` indexes the face's bitmap, where the glyph's rows
// are packed most-significant bit first with no padding between rows. `xOffset` and `yOffset` place
// the glyph's top-left relative to the cursor's baseline.
struct Glyph {
  uint16_t bitmapOffset;
  uint8_t width;
  uint8_t height;
  uint8_t xAdvance;
  int8_t xOffset;
  int8_t yOffset;
};

// A face in one of two kinds, which `kind` names and which decides the fields that are read.
//
// A fixed face is one size for every byte value from 0 to 255, drawn straight from a packed bitmap:
// the glyph for code c is the `width` bytes at bitmap[c * width], one byte to a column, and in each
// byte the least significant bit is the top row. That is glcdfont.c's own packing, so a classic
// 5x7 table is a fixed face without conversion. Nothing else is stored: the advance is the width
// plus the blank column between cells, and the face's last row sits on the baseline. One byte holds
// a column's eight rows, which is the most a fixed face can have.
//
// A variable face draws the glyph for code c from glyph[c - first] at that glyph's own offsets and
// moves the cursor by that glyph's own advance, so a run of text moves by whatever each glyph says.
//
// Both kinds carry their line box: `ascent` rows above the baseline and `descent` rows on and below
// it, which is what a face's height is without a string to measure. A glyph may reach past it.
struct Font {
  enum class Kind : uint8_t { Fixed, Variable };

  Kind kind;
  const uint8_t* bitmap;
  uint8_t width;       // fixed: the cell's columns, one byte of `bitmap` each
  uint8_t height;      // fixed: the cell's rows, one bit of a column each
  const Glyph* glyph;  // variable: the glyph table
  uint16_t first;      // variable: the first code the table holds
  uint16_t last;       // variable: the last
  uint8_t ascent;      // the rows above the baseline
  uint8_t descent;     // the rows on and below it
};

// The rows one line of the face takes, which is the height a band has to hold for it.
inline uint8_t lineHeight(const Font& face) {
  return static_cast<uint8_t>(face.ascent + face.descent);
}

}  // namespace gardener::gfx