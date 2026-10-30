#include "panel/page/MarkScene.h"

#include "panel/page/Mark.h"

namespace gardener::panel::page {

void MarkScene::drawPage(gfx::Canvas& canvas) {
  canvas.draw(gfx::shapes::MonoBitmap(0, 0, MarkBits, gfx::Ink));
}

}  // namespace gardener::panel::page
