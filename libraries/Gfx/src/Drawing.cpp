#include "Drawing.h"

#include <stdio.h>

#include <string>
#include <vector>

#include "Shapes.h"
#include "Text.h"

namespace gardener::gfx {
namespace {

// Gfx draws a string from its baseline. The built-in face is drawn from the top of a five-by-seven
// cell, so its baseline is seven rows below the corner; a compiled face is measured instead, and
// its ink height is what moves the corner to the baseline.
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

Paint text(Rect box, const char* value, const Font* face, uint8_t scale) {
  const char* string = value == nullptr ? "" : value;
  if (scale == 0) scale = 1;
  return [box, string, face, scale](Canvas& canvas) {
    View own = canvas.child(box);
    own.draw(shapes::Text(0, baseline(string, face, scale), string, face, scale, Ink));
  };
}

void drawCentered(Canvas& canvas, const char* value, const Font* face, uint8_t scale) {
  const char* string = value == nullptr ? "" : value;
  if (scale == 0) scale = 1;
  const shapes::TextBox ink = shapes::measureText(face, string, scale);
  const int16_t x = static_cast<int16_t>((canvas.width() - ink.width) / 2);
  const int16_t y = static_cast<int16_t>((canvas.height() - ink.height) / 2);
  text(Rect(x, y, ink.width, ink.height), string, face, scale)(canvas);
}

Paint icon(Rect box, Paint glyph) {
  return [box, glyph](Canvas& canvas) {
    View own = canvas.child(box);
    glyph(own);
  };
}

Paint icon(Rect box, const char* glyph) {
  return [box, glyph](Canvas& canvas) {
    const std::vector<uint8_t> bits = shapes::monoBitmapBits(glyph);
    if (bits.empty()) return;
    View own = canvas.child(box);
    own.draw(shapes::MonoBitmap(0, 0, bits.data(), Ink));
  };
}

Paint inverted(Paint content) {
  if (!content) return Paint();
  return [content](Canvas& canvas) {
    // Ink under the content, and the content drawn through an inverting blend, is the picture the
    // old draw-then-flip made: the content's ink comes out as paper on ink. It is also the only
    // order a blend allows, since a blend sees the value being written and never the frame.
    canvas.draw(shapes::Rect(0, 0, canvas.width(), canvas.height(), Ink, true));
    View flipped = canvas.child(Transform(Rect(0, 0, canvas.width(), canvas.height())),
                                  Blend().invert());
    content(flipped);
  };
}

Paint underlay(Paint child) {
  if (!child) return Paint();
  return [child](Canvas& canvas) {
    canvas.draw(shapes::Rect(0, 0, canvas.width(), canvas.height(), Paper, true));
    child(canvas);
  };
}

Paint progressBar(Rect box, float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 100.0f) value = 100.0f;
  return [box, value](Canvas& canvas) {
    View own = canvas.child(box);
    const int16_t width = own.width();
    const int16_t height = own.height();
    own.draw(shapes::Rect(0, 0, width, height, Ink));
    const int16_t fill = static_cast<int16_t>((width - 2) * value / 100.0f);
    if (fill > 0) own.draw(shapes::Rect(1, 1, fill, height - 2, Ink, true));
  };
}

Paint scrollbar(Rect box, int16_t index, int16_t count, int16_t visible, bool hollow) {
  return [box, index, count, visible, hollow](Canvas& canvas) {
    if (count <= 0) return;
    View own = canvas.child(box);
    const int16_t width = own.width();
    const int16_t height = own.height();
    int16_t thumb = static_cast<int16_t>(height * visible / count);
    if (thumb < 1) thumb = 1;
    if (thumb > height) thumb = height;
    const int16_t span = count - visible;
    int16_t top = 0;
    if (span > 0) top = static_cast<int16_t>((height - thumb) * index / span);
    if (top < 0) top = 0;
    if (top + thumb > height) top = static_cast<int16_t>(height - thumb);
    if (hollow) {
      // The track is untouched paper and inverting paper is ink, so a fill is the same picture
      // without a read the blend cannot make.
      const int16_t below = static_cast<int16_t>(top + thumb);
      own.draw(shapes::Rect(0, 0, width, top, Ink, true));
      own.draw(shapes::Rect(0, below, width, static_cast<int16_t>(height - below), Ink, true));
    } else {
      own.draw(shapes::Rect(0, 0, width, height, Ink));
      own.draw(shapes::Rect(0, top, width, thumb, Ink, true));
    }
  };
}

void drawScrollbar(Canvas& canvas, int16_t index, int16_t count, int16_t visible) {
  if (count <= visible) return;
  constexpr int16_t Width = 2;
  constexpr int16_t Inset = 2;
  const int16_t tall = static_cast<int16_t>(canvas.height() - 2 * Inset);
  const int16_t left = static_cast<int16_t>(canvas.width() - Width);
  scrollbar(Rect(left, Inset, Width, tall), index, count, visible, true)(canvas);
}

Paint duration(Rect box, DurationFormat format, int32_t seconds, const Font* face, bool known) {
  char out[16];
  durationText(format, seconds, known, out, sizeof(out));
  const std::string formatted(out);
  return [box, formatted, face](Canvas& canvas) {
    View own = canvas.child(box);
    const char* value = formatted.c_str();
    own.draw(shapes::Text(0, baseline(value, face, 1), value, face, 1, Ink));
  };
}

}  // namespace gardener::gfx
