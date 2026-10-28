# Fonts

The compiled faces the Gfx library ships. Each header converts an Adafruit_GFX Fonts header into the
library's own `Font` and `Glyph` structures. Fonts.cpp names the faces and Fonts.h declares the
aliases.

- The FreeSans and FreeMono families are Adafruit's generated headers from GNU FreeFont.
  Adafruit_GFX's BSD license is in LICENSE-Adafruit.txt.
- TomThumb carries its own BSD notice in its header. Picopixel is attributed to Sebastian Weber.
- Org_01 and Tiny3x3a2pt7b are not ported: Tiny3x3a2pt7b is CC BY-NC-SA, and neither face is used by
  the firmware.

A face sets fixedWidth, fixedHeight and fixedAdvance when every glyph in its range shares that
metric; otherwise the field is zero.
