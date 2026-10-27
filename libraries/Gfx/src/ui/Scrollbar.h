#pragma once

#include "ui/Component.h"

namespace gardener::gfx::ui {

// A scrollbar: a thumb sized and placed from the index showing, the count of rows there are and the
// rows that fit. Nothing is drawn when there are no rows at all.
//
// `hollow` is the variant a page uses inside its own frame: the track is not outlined and filled,
// it is inverted above and below the thumb, so the thumb reads as the part that was left alone.
class Scrollbar : public Component {
 public:
  bool hollow() const { return hollow_; }
  void setHollow(bool hollow) { hollow_ = hollow; }

  int16_t index() const { return index_; }
  void setIndex(int16_t index) { index_ = index; }

  int16_t count() const { return count_; }
  void setCount(int16_t count) { count_ = count; }

  int16_t visible() const { return visible_; }
  void setVisible(int16_t visible) { visible_ = visible; }

  // The top and the height of the thumb this bar draws, which is what a caller asserting a scroll
  // position reads.
  int16_t thumbTop() const;
  int16_t thumbHeight() const;

  void draw(Canvas& canvas) override;

 private:
  bool hollow_ = false;
  int16_t index_ = 0;
  int16_t count_ = 0;
  int16_t visible_ = 0;
};

}  // namespace gardener::gfx::ui
