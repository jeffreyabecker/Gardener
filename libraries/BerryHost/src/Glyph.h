#pragma once

#include <stdint.h>

#include <vector>

namespace gardener {
namespace berry {

// A glyph is a packed one-bit bitmap — the width, the height, then one row per byte-run, most
// significant bit first and each row padded to a whole byte — and the image carries it as the hex
// digits that name it, two per byte. Neither side holds the bytes in between: the panel reads them
// where it paints the glyph.
//
// Whether a string names a glyph: hex digits, an even count, a width and a height that are not zero,
// and at least the rows the height declares. Nothing is allocated, so a caller that refuses a string
// with be_raise has no value to drop on the way out.
bool isGlyph(const char* text);

// The bytes a glyph string names, in the order MonoBitmap reads them, and an empty answer for a
// string that isGlyph refuses.
std::vector<uint8_t> glyphBits(const char* text);

}  // namespace berry
}  // namespace gardener
