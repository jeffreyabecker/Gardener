#pragma once

#include <Gfx.h>

#include "panel/page/Scene.h"

namespace gardener::panel::glass {

// The panel's own page, and the stack's root until the home page is written: the device's mark on the
// page, with the band drawn under it by the stack. It answers no gesture and holds no state.
class MarkScene : public panel::page::Scene {
 public:
  explicit MarkScene(panel::page::GestureQueue& gestures) : Scene(gestures) {}

 protected:
  void drawPage(gfx::Canvas& canvas) override;
};

}  // namespace gardener::panel::glass
