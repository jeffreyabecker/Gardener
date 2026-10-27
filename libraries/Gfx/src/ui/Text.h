#pragma once

#include "gfx/Font.h"
#include "ui/Component.h"

namespace gardener::gfx::ui {

// A string at the face the caller named, drawn at the size it was given in the box the layout
// places it in.
//
// The face is a compiled one, or nullptr for the built-in 5x7 — the face that carries the arrow
// glyphs an editor's hint prints. `scale` is the size that face is drawn at, 1 for the face as
// designed. Nothing here fits a face to a box or keeps a ladder of them: the caller picked the face
// and the size, so the string is drawn whole, and the box is what the layout holding it makes of
// the ink.
//
// The value is a NUL-terminated run of bytes the caller keeps alive for as long as the text draws.
class Text : public Component {
 public:
  explicit Text(const char* value = "", const Font* face = nullptr, uint8_t scale = 1);

  void setValue(const char* value) { value_ = value == nullptr ? "" : value; }
  const char* value() const { return value_; }
  void setFace(const Font* face) { face_ = face; }
  const Font* face() const { return face_; }
  void setScale(uint8_t scale) { scale_ = scale == 0 ? 1 : scale; }
  uint8_t scale() const { return scale_; }

  // The room the string takes at its face, no larger than the cell it was given.
  Size use(int16_t width, int16_t height) const override;

  void draw(Canvas& canvas) override;

  // The ink box the string takes at its face and scale.
  Size ink() const;

 protected:
  const char* value_;
  const Font* face_;
  uint8_t scale_;
};

}  // namespace gardener::gfx::ui
