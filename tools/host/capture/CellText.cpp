#include "CellText.h"

#include "Canvas.h"
#include "fonts/AdafruitFont.h"
#include "layout/Drawing.h"
#include "shapes/Text.h"

namespace gardener {

namespace {

// Whether the built-in face has no ink for a code, so a label can fall back rather than print a
// blank where a glyph is missing. A column of the cell is a byte of the face's bitmap, so a glyph
// with no bit set in any of its columns has nothing to draw.
bool glyphBlank(uint8_t c) {
  const uint8_t* glyph = gfx::AdafruitFont.bitmap;
  const uint16_t start = static_cast<uint16_t>(c * gfx::AdafruitFont.width);
  for (uint8_t across = 0; across < gfx::AdafruitFont.width; across++) {
    if (glyph[start + across] != 0) return false;
  }
  return true;
}

}  // namespace

int16_t cellTextWidth(const char* text) {
  int16_t width = 0;
  if (text == nullptr) return 0;
  for (const char* c = text; *c != '\0'; c++)
    width = static_cast<int16_t>(width + CellWidth);
  return width;
}

void drawCellText(gfx::Canvas& canvas, int16_t x, int16_t y, const char* text) {
  if (text == nullptr) return;
  for (const char* c = text; *c != '\0'; c++) {
    const uint8_t code = static_cast<uint8_t>(*c);
    // A space is the cell's own blank rather than a glyph the face is missing, which matters to a
    // label of more than one word: the fallback below would spell it out as a letter.
    if (code != ' ') {
      const char value[] = {static_cast<char>(glyphBlank(code) ? 'x' : code), '\0'};
      gfx::text(gfx::Box(x, y, 0, gfx::lineHeight(gfx::AdafruitFont), true, false), value,
                gfx::findFamily("adafruit"), gfx::Justify::Start, gfx::Justify::Start)
          ->draw(canvas);
    }
    x = static_cast<int16_t>(x + CellWidth);
  }
}

}  // namespace gardener
