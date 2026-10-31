#include "panel/page/MarkScene.h"

#include "panel/page/Mark.h"

namespace gardener {

void MarkScene::drawPage(gfx::Canvas& canvas) {
  canvas.draw(gfx::MonoBitmap(0, 0, MarkBits, gfx::Ink));
}

}  // namespace gardener