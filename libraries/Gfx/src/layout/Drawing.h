#pragma once

#include <stdint.h>
#include <stddef.h>

#include <Arduino.h>

#include "Paint.h"
#include "../Blend.h"
#include "../Font.h"

namespace gardener::gfx {

// The drawing vocabulary: every factory here answers a node that draws itself in the box it
// declares, and every box may leave a side whole so a leaf fills the cell a layout gave it without
// the page computing a number.

// The two colours a component draws in, whichever layout the frame has.
constexpr uint8_t Ink = 255;
constexpr uint8_t Paper = 0;

// The largest unit a duration shows.
enum class DurationFormat : uint8_t { Minutes, Seconds, Hours };

// The text a count of seconds prints in a format, written into `out`, which the caller keeps:
// `4m30s` for minutes, `30s` for seconds, `1h2m3s` for hours, and `--` for a count that is not
// known. A count of zero or less is `0s` in every format.
const char* durationText(DurationFormat format, int32_t seconds, bool known, char* out,
                         size_t capacity);

// --- the leaves -------------------------------------------------------------

// A string in the box it declares, owned: the node keeps the text it was handed. The copy costs one
// allocation a node, which is what a page that builds its text needs to be safe.
class TextNode : public PaintNode {
 public:
  TextNode(Box box, String value, const Font* face, uint8_t scale);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  String value_;
  const Font* face_;
  uint8_t scale_;
};

// The same string borrowed: the pointer must outlive the node, which a literal or a scene's own
// member already does. No allocation, so it is the node for text that is static.
class BorrowedTextNode : public PaintNode {
 public:
  BorrowedTextNode(Box box, const char* value, const Font* face, uint8_t scale);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  const char* value_;
  const Font* face_;
  uint8_t scale_;
};

// One glyph of the built-in face by its code, for the symbols above the printable range.
class GlyphNode : public PaintNode {
 public:
  GlyphNode(Box box, uint8_t code, uint8_t scale);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  uint8_t code_;
  uint8_t scale_;
};

// A fixed bitmap named by hex digits: two per byte, the width, the height, then one row per
// byte-run. The string is the caller's and outlives the draw; one that names no bitmap draws
// nothing.
class BitmapNode : public PaintNode {
 public:
  BitmapNode(Box box, const char* glyph);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  const char* glyph_;
};

// A glyph drawn inside the box it declares, by a paint of its own.
class IconNode : public PaintNode {
 public:
  IconNode(Box box, PaintNodeRef glyph);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  PaintNodeRef glyph_;
};

// A framed bar filled to a percentage, held to either end.
class ProgressBarNode : public PaintNode {
 public:
  ProgressBarNode(Box box, float value);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  float value_;
};

// The thumb that shows where a window sits in a list.
class ScrollbarNode : public PaintNode {
 public:
  ScrollbarNode(Box box, int16_t index, int16_t count, int16_t visible, bool hollow);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  int16_t index_;
  int16_t count_;
  int16_t visible_;
  bool hollow_;
};

// Whole seconds in a format, formatted once into a fixed buffer rather than into a string.
class DurationNode : public PaintNode {
 public:
  DurationNode(Box box, DurationFormat format, int32_t seconds, const Font* face, bool known);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  const Font* face_;
  char text_[16];
};

// The seam: a draw the vocabulary cannot name, for a page whose content depends on something only
// known as it draws.
class CallableNode : public PaintNode {
 public:
  explicit CallableNode(std::function<void(Canvas&)> draw);

  void draw(Canvas& canvas) const override;

 private:
  std::function<void(Canvas&)> draw_;
};

// --- the composites ---------------------------------------------------------

// A child drawn through a blend: inverted, dimmed, or a chain of both.
class BlendNode : public PaintNode {
 public:
  BlendNode(Blend blend, PaintNodeRef child);

  void draw(Canvas& canvas) const override;

 private:
  Blend blend_;
  PaintNodeRef child_;
};

// A child drawn over paper cleared across the whole cell, so what it does not ink reads as paper. An
// empty child clears nothing.
class UnderlayNode : public PaintNode {
 public:
  explicit UnderlayNode(PaintNodeRef child);

  void draw(Canvas& canvas) const override;

 private:
  PaintNodeRef child_;
};

// --- the factories ----------------------------------------------------------

PaintNodeRef text(Box box, String value, const Font* face = nullptr, uint8_t scale = 1);
PaintNodeRef borrowedText(Box box, const char* value, const Font* face = nullptr,
                          uint8_t scale = 1);
PaintNodeRef glyph(Box box, uint8_t code, uint8_t scale = 1);
PaintNodeRef icon(Box box, const char* bits);
PaintNodeRef icon(Box box, PaintNodeRef glyph);
PaintNodeRef progressBar(Box box, float value);
PaintNodeRef scrollbar(Box box, int16_t index, int16_t count, int16_t visible,
                       bool hollow = false);
PaintNodeRef duration(Box box, DurationFormat format, int32_t seconds, const Font* face = nullptr,
                      bool known = true);
PaintNodeRef blend(Blend blend, PaintNodeRef child);
PaintNodeRef underlay(PaintNodeRef child);
PaintNodeRef callable(std::function<void(Canvas&)> draw);

// A child drawn as paper on ink: the cell filled with ink and the child drawn through an inverting
// blend, so its ink comes out as paper. An empty child draws nothing.
PaintNodeRef inverted(PaintNodeRef child);

// The width a scrolling band gives up to its scrollbar: the bar's two pixels and the two pixels of
// paper between it and the rows.
constexpr int16_t ScrollbarColumn = 4;

// The scrollbar a scrolling band carries down its right, two pixels wide with two pixels of paper
// above and below. Nothing is drawn when every row fits, so a caller may place the rows over the
// whole width and call this last.
void drawScrollbar(Canvas& canvas, int16_t index, int16_t count, int16_t visible);

}  // namespace gardener::gfx
