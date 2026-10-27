#include "ui/Layout.h"

namespace gardener::gfx::ui {

Layout::Layout(const Cell& cell, const Alignment& alignment)
    : cell_(cell), alignment_(alignment) {}

bool Layout::add(Component& child) {
  if (count_ >= MaxChildren) return false;
  children_[count_++] = &child;
  return true;
}

bool Layout::setChildren(Component* const* children, size_t count) {
  if (count > MaxChildren) return false;
  for (size_t i = 0; i < count; i++) children_[i] = children[i];
  count_ = count;
  return true;
}

Component* Layout::child(size_t index) const {
  return index < count_ ? children_[index] : nullptr;
}

Size Layout::use(int16_t width, int16_t height) const { return {width, height}; }

float Layout::declaredSpan(const Component& child, bool down) const {
  const Cell* declared = child.declaredCell();
  if (declared == nullptr) return Cell::Whole;
  return down ? declared->height : declared->width;
}

bool Layout::stacksDown() const {
  for (size_t i = 0; i < count_; i++) {
    const Cell* declared = children_[i]->declaredCell();
    if (declared == nullptr) continue;
    if (declared->width != Cell::Whole && declared->width != 1.0f) return false;
  }
  return true;
}

void Layout::place() {
  const bool down = stacksDown();
  const int16_t extent = down ? box_.height : box_.width;

  int16_t spans[MaxChildren] = {};
  int16_t consumed = 0;
  for (size_t i = 0; i < count_; i++) {
    int16_t span = resolveSpan(extent, declaredSpan(*children_[i], down));
    if (span > extent - consumed) span = extent - consumed;
    if (span < 0) span = 0;
    spans[i] = span;
    consumed = static_cast<int16_t>(consumed + span);
  }

  const int16_t leftover = static_cast<int16_t>(extent - consumed);
  const int16_t start = down ? box_.y : box_.x;
  int16_t cursor = static_cast<int16_t>(
      start + (down ? alignedOffset(leftover, alignment_.vertical)
                    : alignedOffset(leftover, alignment_.horizontal)));

  for (size_t i = 0; i < count_; i++) {
    const Cell* declared = children_[i]->declaredCell();
    if (down) {
      const int16_t wide = resolveSpan(box_.width, declared ? declared->width : Cell::Whole);
      const int16_t x =
          static_cast<int16_t>(box_.x + alignedOffset(box_.width - wide, alignment_.horizontal));
      childBox(*children_[i], x, cursor, wide, spans[i]);
    } else {
      const int16_t tall = resolveSpan(box_.height, declared ? declared->height : Cell::Whole);
      const int16_t y =
          static_cast<int16_t>(box_.y + alignedOffset(box_.height - tall, alignment_.vertical));
      childBox(*children_[i], cursor, y, spans[i], tall);
    }
    cursor = static_cast<int16_t>(cursor + spans[i]);
  }
}

void Layout::childBox(Component& child, int16_t x, int16_t y, int16_t w, int16_t h) {
  const Size used = child.use(w, h);
  const int16_t wide = used.width > w ? w : used.width;
  const int16_t tall = used.height > h ? h : used.height;
  x = static_cast<int16_t>(x + alignedOffset(w - wide, alignment_.horizontal));
  y = static_cast<int16_t>(y + alignedOffset(h - tall, alignment_.vertical));
  child.setBox(x, y, wide, tall);
}

void Layout::draw(Canvas& canvas) {
  place();
  for (size_t i = 0; i < count_; i++) children_[i]->draw(canvas);
  Component::draw(canvas);
}

}  // namespace gardener::gfx::ui
