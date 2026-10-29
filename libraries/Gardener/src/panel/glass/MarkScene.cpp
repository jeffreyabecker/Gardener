#include "Build.h"

#if GARDENER_PANEL

#include "panel/glass/MarkScene.h"

#include "panel/glass/Mark.h"

namespace gardener::panel::glass {

void MarkScene::drawPage(gfx::Canvas& canvas) {
  canvas.draw(gfx::shapes::MonoBitmap(0, 0, MarkBits, gfx::Ink));
}

}  // namespace gardener::panel::glass
#endif
