#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>

#include "Canvas.h"
#include "Font.h"
#include "Rect.h"

namespace gardener::gfx {

// The two colours a component draws in, whichever layout the frame has: a Gray8 frame writes the
// byte, a Packed1 one sets a bit for anything at or above 128.
constexpr uint8_t Ink = 255;
constexpr uint8_t Paper = 0;

// A draw the caller runs on the canvas it is drawing on: the box is closed over, the content is
// drawn inside it, and a write past its edge is clipped rather than painted over its neighbour. By
// reference because a Canvas is neither copyable nor movable.
using Paint = std::function<void(Canvas&)>;

// The largest unit a duration shows.
enum class DurationFormat : uint8_t { Minutes, Seconds, Hours };

// The text a count of seconds prints in a format, written into `out`, which the caller keeps:
// `4m30s` for minutes, `30s` for seconds, `1h2m3s` for hours, and `--` for a count that is not
// known. A count of zero or less is `0s` in every format.
const char* durationText(DurationFormat format, int32_t seconds, bool known, char* out,
                         size_t capacity);

// A string at the face the caller named, drawn from the top of `box`. nullptr is the built-in 5x7,
// and `scale` is the size the face is drawn at, 1 for the face as designed. The string is a
// NUL-terminated run the caller keeps alive for as long as the draw runs.
Paint text(Rect box, const char* value, const Font* face = nullptr, uint8_t scale = 1);

// A string centred across and down the canvas it is drawn on, at the face and scale it is given —
// what a band or a row that holds one string wants, where its own extent is the box. nullptr is the
// built-in 5x7, and the string is a NUL-terminated run the caller keeps alive for as long as the
// call runs.
void drawCentred(Canvas& canvas, const char* value, const Font* face = nullptr, uint8_t scale = 1);

// A glyph drawn inside `box` by `glyph`, which draws its ink in its own coordinates.
Paint icon(Rect box, Paint glyph);

// The same from the hex digits that name a packed bitmap — two per byte, the width, the height, then
// one row per byte-run, most significant bit first and each row padded to a whole byte, which is how
// the image carries a glyph. The string is a NUL-terminated run the caller keeps alive for as long
// as the draw runs, and a string that names no bitmap draws nothing.
Paint icon(Rect box, const char* glyph);

// A paint that draws `content` and then inverts the whole canvas it was given, so a selected row
// reads as paper on ink. An empty content stays empty and inverts nothing.
Paint inverted(Paint content);

// A framed bar filled to a percentage: the outline is `box` and the fill is its inside. A value
// outside 0 … 100 is held to the end it passed.
Paint progressBar(Rect box, float value);

// The thumb that shows where a window sits in a list, sized and placed from the index showing, the
// count of rows and the rows that fit. `hollow` inverts the track above and below the thumb instead
// of drawing a frame and a filled thumb. A count of none draws nothing.
Paint scrollbar(Rect box, int16_t index, int16_t count, int16_t visible, bool hollow = false);

// The width a scrolling band gives up to its scrollbar: the bar's two pixels and the two pixels of
// paper between it and the rows.
constexpr int16_t ScrollbarColumn = 4;

// The scrollbar a scrolling band carries down its right, two pixels wide with two pixels of paper
// above and below, its thumb the part of an inverted track left bare. Nothing is drawn when every
// row fits, so a caller may place the rows over the whole width and call this last.
void drawScrollbar(Canvas& canvas, int16_t index, int16_t count, int16_t visible);

// Whole seconds formatted in the panel's units of time and drawn as the text they print. A count
// that is not known draws `--`.
Paint duration(Rect box, DurationFormat format, int32_t seconds, const Font* face = nullptr,
               bool known = true);

}  // namespace gardener::gfx
