# Fonts

The compiled faces the Gfx library ships. Each header converts Adafruit_GFX's Fonts header, or a UI
symbol face, into the library's own `Font` and `Glyph` structures. Fonts.cpp names the faces and
Fonts.h declares the aliases.

- The FreeSans, FreeMono and FreeSerif families are Adafruit's generated headers from GNU FreeFont.
  Adafruit_GFX's BSD license is in LICENSE-Adafruit.txt.
- The UISymbol faces are vendored from gilesp1729/FontCollection under MPL-2.0; see
  LICENSE-MPL-2.0.txt.
- TomThumb carries its own BSD notice in its header. Picopixel is attributed to Sebastian Weber.
- Org_01 and Tiny3x3a2pt7b are not ported: Tiny3x3a2pt7b is CC BY-NC-SA, and neither face is used by
  the firmware.

A face sets fixedWidth, fixedHeight and fixedAdvance when every glyph in its range shares that
metric; otherwise the field is zero.
