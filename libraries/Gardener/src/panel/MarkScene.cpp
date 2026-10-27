#include "Build.h"

#if GARDENER_PANEL

#include "panel/MarkScene.h"

#include "panel/Mark.h"

namespace gardener {

void MarkScene::drawPage(gfx::Canvas& canvas) {
  canvas.draw(gfx::shapes::MonoBitmap(0, 0, MarkBits, gfx::ui::Ink));
}

}  // namespace gardener
#endif
