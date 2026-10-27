#pragma once

#include "ui/Component.h"

namespace gardener::gfx::ui {

// A framed bar filled to a percentage: the outline is the box and the fill is its inside, so the
// value reads as the share of the frame that is inked. A value outside 0 … 100 is held to those
// ends rather than refused.
class ProgressBar : public Component {
 public:
  explicit ProgressBar(float value = 0.0f);

  void setValue(float value) { value_ = value; }
  float value() const { return value_; }

  void draw(Canvas& canvas) override;

 protected:
  float value_;
};

}  // namespace gardener::gfx::ui
