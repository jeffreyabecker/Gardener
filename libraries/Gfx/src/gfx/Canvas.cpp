#include "gfx/Canvas.h"

namespace gardener::gfx {

Canvas Canvas::child(Rect area) const {
  // The area is this canvas's own coordinates, so it is clamped to the space the canvas draws in
  // rather than to the panel: a child never reaches a pixel its parent could not.
  int16_t left = area.x < 0 ? 0 : area.x;
  int16_t top = area.y < 0 ? 0 : area.y;
  int16_t right = static_cast<int16_t>(area.x + area.width);
  int16_t bottom = static_cast<int16_t>(area.y + area.height);
  const int16_t spaceWidth = width();
  const int16_t spaceHeight = height();
  if (left > spaceWidth) left = spaceWidth;
  if (top > spaceHeight) top = spaceHeight;
  if (right > spaceWidth) right = spaceWidth;
  if (bottom > spaceHeight) bottom = spaceHeight;

  if (right <= left || bottom <= top) {
    int16_t panelX = 0;
    int16_t panelY = 0;
    toPanel(left, top, panelX, panelY);
    return Canvas(buffer_, panel_, Rect(panelX, panelY, 0, 0), layout_, rotation_, dirtyTarget_);
  }

  // The extent the child draws into is the two opposite corners of the area in the panel's own
  // coordinates: a rectangle stays a rectangle under a quarter turn, with its sides swapped.
  int16_t x1 = 0;
  int16_t y1 = 0;
  int16_t x2 = 0;
  int16_t y2 = 0;
  toPanel(left, top, x1, y1);
  toPanel(static_cast<int16_t>(right - 1), static_cast<int16_t>(bottom - 1), x2, y2);
  const int16_t minX = x1 < x2 ? x1 : x2;
  const int16_t minY = y1 < y2 ? y1 : y2;
  const int16_t maxX = x1 > x2 ? x1 : x2;
  const int16_t maxY = y1 > y2 ? y1 : y2;
  return Canvas(buffer_, panel_,
                Rect(minX, minY, static_cast<int16_t>(maxX - minX + 1),
                     static_cast<int16_t>(maxY - minY + 1)),
                layout_, rotation_, dirtyTarget_);
}

void Canvas::setRotation(int16_t degrees) {
  // A child's extent was resolved for the turn it inherited, so a further turn would draw outside
  // the rectangle its parent gave it.
  if (!owner()) return;
  const int16_t turn = static_cast<int16_t>(((degrees % 360) + 360) % 360);
  if (turn % 90 != 0) return;
  rotation_ = static_cast<uint16_t>(turn);
}

void Canvas::flush(DisplayDevice& device) {
  device.flush(*dirtyTarget_);
  *dirtyTarget_ = Rect(0, 0, 0, 0);
}

void Canvas::include(int16_t panelX, int16_t panelY) {
  Rect& dirty = *dirtyTarget_;
  if (dirty.width <= 0 || dirty.height <= 0) {
    dirty = Rect(panelX, panelY, 1, 1);
    return;
  }
  const int16_t left = panelX < dirty.x ? panelX : dirty.x;
  const int16_t top = panelY < dirty.y ? panelY : dirty.y;
  const int16_t right = panelX > dirty.right() ? panelX : dirty.right();
  const int16_t bottom = panelY > dirty.bottom() ? panelY : dirty.bottom();
  dirty = Rect(left, top, static_cast<int16_t>(right - left + 1),
               static_cast<int16_t>(bottom - top + 1));
}

void Canvas::toPanel(int16_t x, int16_t y, int16_t& panelX, int16_t& panelY) const {
  switch (rotation_) {
    case 90:
      // The canvas's x runs down the panel and its y runs back across it, so the top-left of the
      // turned page is the top-right of the extent.
      panelX = extent_.x + extent_.width - 1 - y;
      panelY = extent_.y + x;
      break;
    case 180:
      panelX = extent_.x + extent_.width - 1 - x;
      panelY = extent_.y + extent_.height - 1 - y;
      break;
    case 270:
      panelX = extent_.x + y;
      panelY = extent_.y + extent_.height - 1 - x;
      break;
    default:
      panelX = extent_.x + x;
      panelY = extent_.y + y;
      break;
  }
}

void Canvas::setPixel(int16_t x, int16_t y, uint8_t color) {
  if (!contains(x, y)) return;
  int16_t panelX = 0;
  int16_t panelY = 0;
  toPanel(x, y, panelX, panelY);
  include(panelX, panelY);
  if (layout_ == PixelLayout::Gray8) {
    buffer_[panelY * stride() + panelX] = color;
    return;
  }
  uint8_t& cell = buffer_[panelY * stride() + (panelX >> 3)];
  const uint8_t mask = 0x80 >> (panelX & 7);
  if (color >= 128) {
    cell |= mask;
  } else {
    cell &= static_cast<uint8_t>(~mask);
  }
}

void Canvas::invert(Rect area) {
  for (int16_t row = 0; row < area.height; row++) {
    for (int16_t col = 0; col < area.width; col++) {
      const int16_t pixelX = area.x + col;
      const int16_t pixelY = area.y + row;
      if (!contains(pixelX, pixelY)) continue;
      int16_t panelX = 0;
      int16_t panelY = 0;
      toPanel(pixelX, pixelY, panelX, panelY);
      include(panelX, panelY);
      if (layout_ == PixelLayout::Gray8) {
        uint8_t& pixel = buffer_[panelY * stride() + panelX];
        pixel = 255 - pixel;
      } else {
        buffer_[panelY * stride() + (panelX >> 3)] ^= static_cast<uint8_t>(0x80 >> (panelX & 7));
      }
    }
  }
}

}  // namespace gardener::gfx
