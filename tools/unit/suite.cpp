#include "suite.h"

namespace gardener::gfx::test {

StubDisplay display(PageWidth, PageHeight);
Canvas canvas = display.rootCanvas();

void reset() {
  display.clearFrame();
  display.resetFlushes();
}

}  // namespace gardener::gfx::test
