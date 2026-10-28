#include "Build.h"

#if GARDENER_PANEL

#include "Glyph.h"

#include <string.h>

namespace gardener {
namespace berry {
namespace {

// One hex digit as the value it names, or -1 for a character that is not one.
int hexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}

// The byte two digits name, which the caller has already read as hex.
uint8_t hexByte(const char* text) {
  return (uint8_t)((hexDigit(text[0]) << 4) | hexDigit(text[1]));
}

}  // namespace

bool isGlyph(const char* text) {
  if (text == nullptr) return false;
  const size_t count = strlen(text);
  if (count < 4 || count % 2 != 0) return false;
  for (size_t i = 0; i < count; i++) {
    if (hexDigit(text[i]) < 0) return false;
  }

  const uint8_t width = hexByte(text);
  const uint8_t height = hexByte(text + 2);
  if (width == 0 || height == 0) return false;
  const size_t rows = ((size_t)width + 7) / 8 * height;
  return count / 2 >= 2 + rows;
}

std::vector<uint8_t> glyphBits(const char* text) {
  std::vector<uint8_t> bits;
  if (!isGlyph(text)) return bits;

  const size_t count = strlen(text);
  bits.reserve(count / 2);
  for (size_t i = 0; i < count; i += 2) bits.push_back(hexByte(text + i));
  return bits;
}

}  // namespace berry
}  // namespace gardener
#endif
