# Fonts

Four designs live here, in this library's own `Font` and `Glyph` structures, and `Fonts.h` is what a
caller asks about: which face a page that names none draws in, which names a face answers to, and
which cuts a family has.

A `Font` is one of two kinds, and its `kind` says which fields are read. A **fixed** face is one
size for all 256 byte values and carries only a packed bitmap, a width and a height: the glyph for a
code is its width's bytes at `bitmap[code * width]`, one byte to a column with the least significant
bit the top row, which is how glcdfont.c packs a face. Nothing else is stored — the advance is the
width plus the blank column between cells, and the last row sits on the baseline — so a fixed face
costs a byte a column and no glyph table at all. A **variable** face is a table of glyphs, each with
its own ink, offsets and advance, which is what a proportional bitmap face needs. Both kinds carry
their line box: the rows above the baseline and the rows on and below it, which is a face's height
without a string to measure.

- **The Adafruit 5x7** (`AdafruitFont.h`/`.cpp`) is the face every page drew in before there was
  another: the 1280 bytes of Adafruit_GFX's glcdfont.c, taken exactly as that file has them, so the
  table can be compared against the original byte for byte. It is a fixed face of five columns by
  eight rows, six pixels apart, and Adafruit's BSD notice is in the header of AdafruitFont.cpp. Every
  byte value has a glyph, which is why it measures as a run of cells rather than as the ink of its
  glyphs.
- **UW ttyp0** (`Ttyp011`, `Ttyp016`, `Ttyp022`) is a real bitmap face at 11, 16 and 22 pixels, from
  u8g2's single-font sources of Uwe Waldmann's ttyp0 family; its licence is in LICENSE-ttyp0.txt. Each
  is a variable face over the codes 0x20 to 0xFF of the u8g2 `_tf` set — 0x7F to 0x9F are blanks as
  wide as the advance, which is where u8g2's own set has a gap.
- **ProFont** (`ProFont10`, `ProFont12`, `ProFont17`, `ProFont29`) is Carl Osterwald's, Stephen C.
  Gilardi's and Andrew Welch's ProFont, from u8g2's `u8g2_font_profont*_tf` sources; its MIT licence
  is in LICENSE-profont.txt. All 224 codes are its own, and the four rungs are the sizes a 128x64
  panel's bands can tell apart rather than everything the design has.
- **X11 misc** (`X11Cell5x7`, `X11Cell6x10`, `X11Cell7x14`, `X11Cell10x20`) is the X11
  distribution's public domain fixed font — its own notice is "Public domain font. Share and enjoy."
  — from u8g2's `u8g2_font_*_tf` sources, named by the cell the source is named by because the design
  has no single nominal size. It holds 191 codes over 0x20 to 0xFF; the rest are blanks.

A family is the cuts of one design, shortest first, and `Fonts.h` names the four the library ships:
`adafruit`, `ttyp0`, `profont` and `x11`. A page names the design and a height picks the cut, so the
rungs of a design are a statement about which sizes the product has, and a rung is added when a band
needs one rather than because the source offers it.

`registerFamilyAlias` adds a generic name for a design: an alias is what a page or a record can say
— `body`, say — while a composition chooses the family behind it, so the page draws in whatever
design the composition points the word at. An alias may be registered before its family and answers
nothing until the family lands, registering the same alias again moves it to another family, and an
alias names a family rather than another alias. Aliases and families share one namespace: a family
takes no name an alias holds and an alias takes no family's.

`tools/fonts/from_u8g2.py` converts a u8g2 single-font `.c` file into the pair of files beside these,
so a size or range can be regenerated rather than hand-edited: u8g2 keeps a face packed and
run-length encoded, and the tool decodes it into this library's plain, byte-aligned glyph bitmaps.
Its `--note` is the sentence the new header opens with, so each design carries whose work it is and
which licence travels with it. The twelve shipped faces are written down in `Fonts.cpp`, so a page or
a record can name one; the default face is still the Adafruit 5x7.

