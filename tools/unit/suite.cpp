#include "suite.h"

namespace gardener::gfx::test {

CaptureDisplay display(PageWidth, PageHeight);
Canvas canvas = display.rootCanvas();

void reset() {
  // A case starts with the canvas reporting nothing, so a rectangle left dirty by the case before
  // it cannot land in the first frame this one captures.
  canvas.flush(display);
  display.clear();
}

}  // namespace gardener::gfx::test
