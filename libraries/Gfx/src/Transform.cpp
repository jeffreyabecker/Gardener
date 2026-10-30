#include "Transform.h"
#include <utility>
namespace gardener::gfx
{
  namespace
  {

    Rect intersect(Rect a, Rect b)
    {
      const int16_t left = a.x > b.x ? a.x : b.x;
      const int16_t top = a.y > b.y ? a.y : b.y;
      const int16_t right = a.right() < b.right() ? a.right() : b.right();
      const int16_t bottom = a.bottom() < b.bottom() ? a.bottom() : b.bottom();
      if (right < left || bottom < top)
        return Rect(left, top, 0, 0);
      return Rect(left, top, static_cast<int16_t>(right - left + 1),
                  static_cast<int16_t>(bottom - top + 1));
    }

  } // namespace

  Transform::Transform(Rect window)
      : tx_(window.x),
        ty_(window.y),
        width_(window.width > 0 ? window.width : 0),
        height_(window.height > 0 ? window.height : 0),
        window_(width_ > 0 && height_ > 0 ? window : Rect(window.x, window.y, 0, 0)) {}

  Transform Transform::clip(Rect area) const
  {
    const Rect image = covered(area);
    Transform next = *this;
    next.tx_ = static_cast<int16_t>(tx_ + ux_ * area.x + vx_ * area.y);
    next.ty_ = static_cast<int16_t>(ty_ + uy_ * area.x + vy_ * area.y);
    next.width_ = area.width > 0 ? area.width : 0;
    next.height_ = area.height > 0 ? area.height : 0;
    next.window_ = intersect(window_, image);
    return next;
  }

  Transform Transform::translate(int16_t dx, int16_t dy) const
  {
    Transform next = *this;
    next.tx_ = static_cast<int16_t>(tx_ + ux_ * dx + vx_ * dy);
    next.ty_ = static_cast<int16_t>(ty_ + uy_ * dx + vy_ * dy);
    return next;
  }

  Transform Transform::scale(int16_t factor) const
  {
    Transform next = *this;
    next.ux_ = static_cast<int16_t>(ux_ * factor);
    next.uy_ = static_cast<int16_t>(uy_ * factor);
    next.vx_ = static_cast<int16_t>(vx_ * factor);
    next.vy_ = static_cast<int16_t>(vy_ * factor);
    return next;
  }

  Transform Transform::rotate(int16_t degrees) const
  {
    const int16_t turn = static_cast<int16_t>(((degrees % 360) + 360) % 360);
    Transform next = *this;
    switch (turn)
    {
    case 90:
      // The space's x runs down the footprint and its y runs back across it, so the turned
      // space's origin is the footprint's top-right corner.
      next.tx_ = static_cast<int16_t>(tx_ + ux_ * (width_ - 1));
      next.ty_ = static_cast<int16_t>(ty_ + uy_ * (width_ - 1));
      next.ux_ = vx_;
      next.uy_ = vy_;
      next.vx_ = static_cast<int16_t>(-ux_);
      next.vy_ = static_cast<int16_t>(-uy_);
      next.width_ = height_;
      next.height_ = width_;
      break;
    case 180:
      next.tx_ = static_cast<int16_t>(tx_ + ux_ * (width_ - 1) + vx_ * (height_ - 1));
      next.ty_ = static_cast<int16_t>(ty_ + uy_ * (width_ - 1) + vy_ * (height_ - 1));
      next.ux_ = static_cast<int16_t>(-ux_);
      next.uy_ = static_cast<int16_t>(-uy_);
      next.vx_ = static_cast<int16_t>(-vx_);
      next.vy_ = static_cast<int16_t>(-vy_);
      break;
    case 270:
      next.tx_ = static_cast<int16_t>(tx_ + vx_ * (height_ - 1));
      next.ty_ = static_cast<int16_t>(ty_ + vy_ * (height_ - 1));
      next.ux_ = static_cast<int16_t>(-vx_);
      next.uy_ = static_cast<int16_t>(-vy_);
      next.vx_ = ux_;
      next.vy_ = uy_;
      next.width_ = height_;
      next.height_ = width_;
      break;
    default:
      break;
    }
    return next;
  }

  Transform Transform::flipX() const
  {
    Transform next = *this;
    next.tx_ = static_cast<int16_t>(tx_ + ux_ * (width_ - 1));
    next.ty_ = static_cast<int16_t>(ty_ + uy_ * (width_ - 1));
    next.ux_ = static_cast<int16_t>(-ux_);
    next.uy_ = static_cast<int16_t>(-uy_);
    return next;
  }

  Transform Transform::flipY() const
  {
    Transform next = *this;
    next.tx_ = static_cast<int16_t>(tx_ + vx_ * (height_ - 1));
    next.ty_ = static_cast<int16_t>(ty_ + vy_ * (height_ - 1));
    next.vx_ = static_cast<int16_t>(-vx_);
    next.vy_ = static_cast<int16_t>(-vy_);
    return next;
  }

  Transform Transform::then(const Transform &outer) const
  {
    Transform next = outer;
    next.ux_ = static_cast<int16_t>(outer.ux_ * ux_ + outer.vx_ * uy_);
    next.uy_ = static_cast<int16_t>(outer.uy_ * ux_ + outer.vy_ * uy_);
    next.vx_ = static_cast<int16_t>(outer.ux_ * vx_ + outer.vx_ * vy_);
    next.vy_ = static_cast<int16_t>(outer.uy_ * vx_ + outer.vy_ * vy_);
    next.tx_ = static_cast<int16_t>(outer.tx_ + outer.ux_ * tx_ + outer.vx_ * ty_);
    next.ty_ = static_cast<int16_t>(outer.ty_ + outer.uy_ * tx_ + outer.vy_ * ty_);
    next.width_ = width_;
    next.height_ = height_;
    // This window is in the outer's own coordinates, so it is only a limit once the outer has
    // carried it onto the panel.
    next.window_ = intersect(outer.window_, outer.covered(window_));
    return next;
  }

  std::pair<int32_t, int32_t> Transform::place(int16_t x, int16_t y) const
  {
    return {tx_ + ux_ * x + vx_ * y, ty_ + uy_ * x + vy_ * y};
  }

  std::pair<uint16_t, uint16_t> Transform::map(int16_t x, int16_t y) const
  {
    const std::pair<int32_t, int32_t> placed = place(x, y);
    return {static_cast<uint16_t>(placed.first), static_cast<uint16_t>(placed.second)};
  }

  Rect Transform::toPanel(Rect area) const { return intersect(covered(area), window_); }

  Rect Transform::covered(Rect area) const
  {
    if (area.width <= 0 || area.height <= 0)
      return Rect(area.x, area.y, 0, 0);
    const std::pair<int32_t, int32_t> corner = place(area.x, area.y);
    const std::pair<int32_t, int32_t> far = place(area.right(), area.bottom());
    const int32_t left = corner.first < far.first ? corner.first : far.first;
    const int32_t top = corner.second < far.second ? corner.second : far.second;
    const int32_t right = corner.first > far.first ? corner.first : far.first;
    const int32_t bottom = corner.second > far.second ? corner.second : far.second;
    return Rect(static_cast<int16_t>(left), static_cast<int16_t>(top),
                static_cast<int16_t>(right - left + 1),
                static_cast<int16_t>(bottom - top + 1));
  }

} // namespace gardener::gfx
