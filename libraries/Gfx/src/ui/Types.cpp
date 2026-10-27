#include "ui/Types.h"

namespace gardener::gfx::ui {
namespace {

// A share of one or less is multiplied out; above that the number is a pixel count.
int16_t pixelsOf(int16_t total, float declared) {
  if (declared > 1.0f) {
    const int16_t span = static_cast<int16_t>(declared);
    return span > total ? total : span;
  }
  return static_cast<int16_t>(total * declared + 0.5f);
}

}  // namespace

int16_t resolveSpan(int16_t total, float declared) {
  if (declared == Cell::Whole) return total;
  const int16_t span = pixelsOf(total, declared);
  return span < 0 ? 0 : span;
}

int16_t alignedOffset(int16_t leftover, Horizontal where) {
  if (leftover <= 0) return 0;
  if (where == Horizontal::Center) return static_cast<int16_t>(leftover / 2);
  if (where == Horizontal::Right) return leftover;
  return 0;
}

int16_t alignedOffset(int16_t leftover, Vertical where) {
  if (leftover <= 0) return 0;
  if (where == Vertical::Center) return static_cast<int16_t>(leftover / 2);
  if (where == Vertical::Bottom) return leftover;
  return 0;
}

}  // namespace gardener::gfx::ui
