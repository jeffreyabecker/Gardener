# Fonts

Two families live here, both converted into this library's own `Font` and `Glyph` structures, and
`Fonts.h` is what a caller asks about either: which face a page that names none draws in, and which
names a face answers to.

- **The built-in 5x7** (`BuiltinFont.h`/`.cpp`) is the face every page drew in before there was
  another: all 256 byte values in a five-by-eight cell, six pixels apart, from Adafruit_GFX 1.12.6's
  glcdfont.c, whose BSD notice is in the header of BuiltinFont.cpp. Every byte value has a glyph,
  which is why it measures as a run of cells rather than as the ink of its glyphs.
- **UW ttyp0** (`Ttyp011`, `Ttyp016`, `Ttyp022`) is a real bitmap face at 11, 16 and 22 pixels, from
  u8g2's single-font sources of Uwe Waldmann's ttyp0 family; its licence is in LICENSE-ttyp0.txt. Each
  covers the codes 0x20 to 0xFF of the u8g2 `_tf` set — 0x7F to 0x9F are blanks as wide as the
  advance, which is where u8g2's own set has a gap.

`tools/fonts/from_u8g2.py` converts a u8g2 single-font `.c` file into the pair of files beside these,
so a size or range can be regenerated rather than hand-edited: u8g2 keeps a face packed and
run-length encoded, and the tool decodes it into this library's plain, byte-aligned glyph bitmaps.
The library registers the three ttyp0 faces as `ttyp011`, `ttyp016` and `ttyp022` as it comes up, so
a page or a record can name one; the default face is still the built-in 5x7.

