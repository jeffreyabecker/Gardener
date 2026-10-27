// The Gfx checks: the drawing functions, the layout, the value editors and the scene stack under
// libraries/Gfx/src/ui/ driven against a stub display on the host, with no interpreter and no
// board. A case asserts numbers a function answers and the pixels it leaves in the frame, and the
// run exits non-zero when one fails. The cases live a file each, named by what they cover.

#include <stdio.h>

#include "suite.h"

namespace {

using namespace gardener::gfx::test;

// Every file's cases, in the order they run: the drawing first, then what is built on it.
std::vector<Case> allCases() {
  std::vector<Case> cases;
  addDrawingCases(cases);
  addLayoutCases(cases);
  addEditorCases(cases);
  addSceneCases(cases);
  addCanvasCases(cases);
  return cases;
}

}  // namespace

int main() {
  int passed = 0;
  int failed = 0;
  for (const Case& item : allCases()) {
    if (runCase(item)) {
      passed++;
    } else {
      failed++;
    }
  }
  printf("%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
