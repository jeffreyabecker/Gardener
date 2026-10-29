#pragma once

#include <Gfx.h>

#include "panel/page/Scene.h"

namespace gardener {

// The panel's own page, and the stack's root until the home page is written: the device's mark on the
// page, with the band drawn under it by the stack. It answers no gesture and holds no state.
class MarkScene : public gfx::ui::Scene {
 public:
  explicit MarkScene(gfx::ui::GestureQueue& gestures) : Scene(gestures) {}

 protected:
  void drawPage(gfx::Canvas& canvas) override;
};

}  // namespace gardener
