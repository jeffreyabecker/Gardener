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

// One glyph of a face. `bitmapOffset` indexes the face's bitmap, where the glyph's rows are packed
// most-significant bit first with no padding between rows. `xOffset` and `yOffset` place the
// glyph's top-left relative to the cursor's baseline.
struct Glyph {
  uint16_t bitmapOffset;
  uint8_t width;
  uint8_t height;
  uint8_t xAdvance;
  int8_t xOffset;
  int8_t yOffset;
};

// A face: the concatenated glyph bitmaps, the glyph table, the character range and the line
// advance. The last three fields describe a face whose glyphs share one size and advance; a zero
// means that metric varies from glyph to glyph. A fixed-size face has a non-zero `fixedWidth` and
// `fixedHeight`; a monospace face has a non-zero `fixedAdvance`.
struct Font {
  const uint8_t* bitmap;
  const Glyph* glyph;
  uint16_t first;
  uint16_t last;
  uint8_t yAdvance;
  uint8_t fixedWidth;
  uint8_t fixedHeight;
  uint8_t fixedAdvance;
};

}  // namespace gardener::gfx
