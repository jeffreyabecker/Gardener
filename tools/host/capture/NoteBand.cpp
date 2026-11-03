#include "NoteBand.h"

#include "Canvas.h"
#include "CellText.h"
#include "Rect.h"
#include "layout/Drawing.h"

namespace gardener {

CaptureDisplay::Component NoteBand::component(gfx::Rect box) {
  return CaptureDisplay::Component{box,
                                   gfx::callable([this](gfx::Canvas& canvas) { draw(canvas); })};
}

void NoteBand::draw(gfx::Canvas& canvas) {
  if (text_.empty()) return;
  // Centred across the band, so a note reads as a caption rather than as a line of the panel. One
  // too wide to fit starts at the band's left edge instead, so what it clips is the end of the
  // sentence rather than both ends of it: the face's size is the whole of what a caption is worth,
  // and shrinking it to make it fit would say the line matters less than the picture under it.
  const int16_t width = cellTextWidth(text_.c_str());
  const int16_t x =
      width <= canvas.width() ? static_cast<int16_t>((canvas.width() - width) / 2) : 0;
  const int16_t y = static_cast<int16_t>((canvas.height() - CellHeight) / 2);
  drawCellText(canvas, x, y, text_.c_str());
}

}  // namespace gardener
