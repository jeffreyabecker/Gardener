#pragma once

#include "ui/Component.h"

namespace gardener::gfx::ui {

// A component a page draws a glyph in: a subclass draws its own ink inside the box it was given and
// ends with Component::draw, so the flip still runs. The default box is the 12 by 8 field a glyph
// is drawn on, and the layout that holds the icon replaces it.
class Icon : public Component {
 public:
  static constexpr int16_t Width = 12;
  static constexpr int16_t Height = 8;

  Icon() { setBox(0, 0, Width, Height); }
};

}  // namespace gardener::gfx::ui
