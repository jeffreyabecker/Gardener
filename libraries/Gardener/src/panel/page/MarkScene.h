#pragma once

#include <Gfx.h>

#include "panel/page/Scene.h"

namespace gardener::panel::page {

// The panel's own page, and the stack's root until the home page is written: the device's mark on
// the page, with the overlay drawn over its bottom rows by the stack. It answers no gesture and
// holds no state.
class MarkScene : public Scene {
 public:
  explicit MarkScene(GestureQueue& gestures) : Scene(gestures) {}

 protected:
  void drawPage(gfx::Canvas& canvas) override;
};

}  // namespace gardener::panel::page
