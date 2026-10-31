# Fonts

The one face the Gfx library ships: the built-in 5x7, converted from Adafruit_GFX 1.12.6's
glcdfont.c into the library's own `Font` and `Glyph` structures. Its BSD notice is in the header of
BuiltinFont.cpp, and Fonts.h is what a caller asks about it: which face a page that names none draws
in, and which name the face answers to.

Every byte value has a glyph, each in a five-by-eight cell six pixels apart, which is why the face
measures as a run of cells rather than as the ink of its glyphs. A page that names a face names this
one as `builtin` or leaves the name out; any other name is refused where the record is made, unless
a caller registered it with `registerFont`, which is how a face that is not compiled in is named.
