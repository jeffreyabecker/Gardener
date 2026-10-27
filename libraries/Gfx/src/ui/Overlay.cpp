#include "ui/Overlay.h"

#include "gfx/Shapes.h"
#include "ui/Layout.h"

namespace gardener::gfx::ui {

void drawBand(Canvas& canvas, const std::vector<Paint>& cells) {
  canvas.draw(shapes::FillRect(0, 0, canvas.width(), canvas.height(), Paper));
  if (cells.empty()) return;
  const float share = 1.0f / static_cast<float>(cells.size());
  std::vector<Child> children;
  children.reserve(cells.size());
  for (const Paint& cell : cells) {
    children.push_back(Child{Cell(share, Cell::Whole), cell});
  }
  row(1.0f, children)(canvas);
}

}  // namespace gardener::gfx::ui
