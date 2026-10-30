#include "Drawing.h"

#include <stdio.h>

#include <memory>
#include <utility>
#include <vector>

#include "../shapes/Shapes.h"
#include "../shapes/Text.h"

namespace gardener::gfx {
namespace {

// Gfx draws a string from its baseline. The built-in face is drawn from the top of a five-by-seven
// cell, so its baseline is seven rows below the corner; a compiled face is measured instead, and its
// ink height is what moves the corner to the baseline.
int16_t baseline(const char* value, const Font* face, uint8_t scale) {
  if (face == nullptr) return static_cast<int16_t>(7 * scale);
  return shapes::measureText(face, value, scale).height;
}

// Appends the part a unit of time contributes, and answers where what has been written ends.
char* append(char* out, size_t capacity, const char* suffix, int32_t count) {
  if (count <= 0) return out;
  size_t used = 0;
  while (out[used] != '\0' && used < capacity) used++;
  if (used >= capacity) return out;
  snprintf(out + used, capacity - used, "%ld%s", static_cast<long>(count), suffix);
  return out;
}

}  // namespace

const char* durationText(DurationFormat format, int32_t seconds, bool known, char* out,
                         size_t capacity) {
  if (out == nullptr || capacity == 0) return "";
  out[0] = '\0';
  if (!known) {
    snprintf(out, capacity, "--");
    return out;
  }
  if (seconds <= 0) {
    snprintf(out, capacity, "0s");
    return out;
  }
  if (format == DurationFormat::Seconds) {
    snprintf(out, capacity, "%lds", static_cast<long>(seconds));
    return out;
  }
  if (format == DurationFormat::Hours) {
    append(out, capacity, "h", seconds / 3600);
    append(out, capacity, "m", (seconds % 3600) / 60);
    append(out, capacity, "s", seconds % 60);
    return out;
  }
  const int32_t minutes = seconds / 60;
  const int32_t rest = seconds % 60;
  append(out, capacity, "m", minutes);
  append(out, capacity, "s", rest);
  return out;
}

// --- the leaves -------------------------------------------------------------

TextNode::TextNode(Box box, String value, const Font* face, uint8_t scale)
    : box_(box), value_(std::move(value)), face_(face), scale_(scale == 0 ? 1 : scale) {}

void TextNode::draw(Canvas& canvas) const {
  View own = canvas.child(box_.in(canvas));
  const char* text = value_.c_str();
  own.draw(shapes::Text(0, baseline(text, face_, scale_), text, face_, scale_, Ink));
}

BorrowedTextNode::BorrowedTextNode(Box box, const char* value, const Font* face, uint8_t scale)
    : box_(box), value_(value == nullptr ? "" : value), face_(face),
      scale_(scale == 0 ? 1 : scale) {}

void BorrowedTextNode::draw(Canvas& canvas) const {
  View own = canvas.child(box_.in(canvas));
  own.draw(shapes::Text(0, baseline(value_, face_, scale_), value_, face_, scale_, Ink));
}

GlyphNode::GlyphNode(Box box, uint8_t code, uint8_t scale)
    : box_(box), code_(code), scale_(scale == 0 ? 1 : scale) {}

void GlyphNode::draw(Canvas& canvas) const {
  View own = canvas.child(box_.in(canvas));
  shapes::drawGlyph(own, 0, 0, code_, scale_, Ink);
}

BitmapNode::BitmapNode(Box box, const char* glyph) : box_(box), glyph_(glyph) {}

void BitmapNode::draw(Canvas& canvas) const {
  const std::vector<uint8_t> bits = shapes::monoBitmapBits(glyph_);
  if (bits.empty()) return;
  View own = canvas.child(box_.in(canvas));
  own.draw(shapes::MonoBitmap(0, 0, bits.data(), Ink));
}

IconNode::IconNode(Box box, PaintNodeRef glyph) : box_(box), glyph_(std::move(glyph)) {}

void IconNode::draw(Canvas& canvas) const {
  View own = canvas.child(box_.in(canvas));
  if (glyph_) glyph_->draw(own);
}

ProgressBarNode::ProgressBarNode(Box box, float value)
    : box_(box), value_(value < 0.0f ? 0.0f : (value > 100.0f ? 100.0f : value)) {}

void ProgressBarNode::draw(Canvas& canvas) const {
  View own = canvas.child(box_.in(canvas));
  const int16_t width = own.width();
  const int16_t height = own.height();
  own.draw(shapes::Rect(0, 0, width, height, Ink));
  const int16_t fill = static_cast<int16_t>((width - 2) * value_ / 100.0f);
  if (fill > 0) own.draw(shapes::Rect(1, 1, fill, height - 2, Ink, true));
}

ScrollbarNode::ScrollbarNode(Box box, int16_t index, int16_t count, int16_t visible, bool hollow)
    : box_(box), index_(index), count_(count), visible_(visible), hollow_(hollow) {}

void ScrollbarNode::draw(Canvas& canvas) const {
  if (count_ <= 0) return;
  View own = canvas.child(box_.in(canvas));
  const int16_t width = own.width();
  const int16_t height = own.height();
  int16_t thumb = static_cast<int16_t>(height * visible_ / count_);
  if (thumb < 1) thumb = 1;
  if (thumb > height) thumb = height;
  const int16_t span = count_ - visible_;
  int16_t top = 0;
  if (span > 0) top = static_cast<int16_t>((height - thumb) * index_ / span);
  if (top < 0) top = 0;
  if (top + thumb > height) top = static_cast<int16_t>(height - thumb);
  if (hollow_) {
    // The track is untouched paper and inverting paper is ink, so a fill is the same picture without
    // a read the blend cannot make.
    const int16_t below = static_cast<int16_t>(top + thumb);
    own.draw(shapes::Rect(0, 0, width, top, Ink, true));
    own.draw(shapes::Rect(0, below, width, static_cast<int16_t>(height - below), Ink, true));
  } else {
    own.draw(shapes::Rect(0, 0, width, height, Ink));
    own.draw(shapes::Rect(0, top, width, thumb, Ink, true));
  }
}

CallableNode::CallableNode(std::function<void(Canvas&)> draw) : draw_(std::move(draw)) {}

void CallableNode::draw(Canvas& canvas) const {
  if (draw_) draw_(canvas);
}

// --- the composites ---------------------------------------------------------

BlendNode::BlendNode(Blend blend, PaintNodeRef child) : blend_(blend), child_(std::move(child)) {}

void BlendNode::draw(Canvas& canvas) const {
  // Ink under the content, and the content drawn through an inverting blend, is the picture the old
  // draw-then-flip made: the content's ink comes out as paper on ink. It is also the only order a
  // blend allows, since a blend sees the value being written and never the frame.
  View blended =
      canvas.child(Transform(Rect(0, 0, canvas.width(), canvas.height())), blend_);
  if (child_) child_->draw(blended);
}

UnderlayNode::UnderlayNode(PaintNodeRef child) : child_(std::move(child)) {}

void UnderlayNode::draw(Canvas& canvas) const {
  canvas.draw(shapes::Rect(0, 0, canvas.width(), canvas.height(), Paper, true));
  if (child_) child_->draw(canvas);
}

// --- the factories ----------------------------------------------------------

PaintNodeRef text(Box box, String value, const Font* face, uint8_t scale) {
  return std::make_unique<TextNode>(box, std::move(value), face, scale);
}

PaintNodeRef borrowedText(Box box, const char* value, const Font* face, uint8_t scale) {
  return std::make_unique<BorrowedTextNode>(box, value, face, scale);
}

PaintNodeRef glyph(Box box, uint8_t code, uint8_t scale) {
  return std::make_unique<GlyphNode>(box, code, scale);
}

PaintNodeRef icon(Box box, const char* bits) {
  return std::make_unique<BitmapNode>(box, bits);
}

PaintNodeRef icon(Box box, PaintNodeRef glyph) {
  return std::make_unique<IconNode>(box, std::move(glyph));
}

PaintNodeRef progressBar(Box box, float value) {
  return std::make_unique<ProgressBarNode>(box, value);
}

PaintNodeRef scrollbar(Box box, int16_t index, int16_t count, int16_t visible, bool hollow) {
  return std::make_unique<ScrollbarNode>(box, index, count, visible, hollow);
}

PaintNodeRef duration(Box box, DurationFormat format, int32_t seconds, const Font* face,
                      bool known) {
  char printed[16];
  durationText(format, seconds, known, printed, sizeof(printed));
  return text(box, String(printed), face, 1);
}

PaintNodeRef blend(Blend blend, PaintNodeRef child) {
  return std::make_unique<BlendNode>(blend, std::move(child));
}

PaintNodeRef underlay(PaintNodeRef child) {
  if (!child) return PaintNodeRef();
  return std::make_unique<UnderlayNode>(std::move(child));
}

PaintNodeRef callable(std::function<void(Canvas&)> draw) {
  return std::make_unique<CallableNode>(std::move(draw));
}

PaintNodeRef inverted(PaintNodeRef child) {
  if (!child) return PaintNodeRef();
  // The underlay clears paper, which the inverting blend writes as ink: the cell comes out filled
  // and the child's ink comes out as paper.
  return blend(Blend().invert(), underlay(std::move(child)));
}

void drawScrollbar(Canvas& canvas, int16_t index, int16_t count, int16_t visible) {
  if (count <= visible) return;
  constexpr int16_t Width = 2;
  constexpr int16_t Inset = 2;
  const int16_t tall = static_cast<int16_t>(canvas.height() - 2 * Inset);
  const int16_t left = static_cast<int16_t>(canvas.width() - Width);
  scrollbar(Rect(left, Inset, Width, tall), index, count, visible, true)->draw(canvas);
}

}  // namespace gardener::gfx
