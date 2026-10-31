#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Arduino.h>

#include "../Blend.h"
#include "../Font.h"
#include "../Fonts.h"
#include "Paint.h"

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

// A string in the box it declares, owned: the node keeps the text it was handed. The box's height
// chooses the cut and scale of the family and the box's width narrows them, so a page names a design
// and a cell and the leaf fills it. The line box is placed by the vertical justification and the
// glyphs sit on its baseline, so two of these in one row share a baseline; the ink is placed across
// by the horizontal one. The family is the module's own table entry, which outlives the tree, and a
// node that names none draws in the default family.
class TextNode : public PaintNode {
 public:
  TextNode(Box box, String value, const Family* family, Justify horizontal, Justify vertical);

  void draw(Canvas& canvas) const override;

 private:
  Box box_;
  String value_;
  const Family* family_;
  Justify horizontal_;
  Justify vertical_;
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

// A string fitted to its box: the cut and scale its height takes, placed by the two justifications,
// which are the centre of the box unless the caller says where. A family that is null is the default
// family, so a leaf that names none still draws.
PaintNodeRef text(Box box, String value, const Family* family = nullptr,
                  Justify horizontal = Justify::Center, Justify vertical = Justify::Center);
PaintNodeRef glyph(Box box, uint8_t code, uint8_t scale = 1);
PaintNodeRef icon(Box box, const char* bits);
PaintNodeRef icon(Box box, PaintNodeRef glyph);
PaintNodeRef progressBar(Box box, float value);
PaintNodeRef scrollbar(Box box, int16_t index, int16_t count, int16_t visible, bool hollow = false);
// Whole seconds printed in a format, as the text node it is: the formatting is all this adds.
PaintNodeRef duration(Box box, DurationFormat format, int32_t seconds,
                      const Family* family = nullptr, bool known = true);
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