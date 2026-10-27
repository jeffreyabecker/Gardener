#include "ui/Drawing.h"

#include <stdio.h>

#include <string>

#include "gfx/Shapes.h"
#include "gfx/Text.h"

namespace gardener::gfx::ui {
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
    Canvas own = canvas.child(box);
    own.draw(shapes::Text(0, baseline(string, face, scale), string, face, scale, Ink));
  };
}

Paint icon(Rect box, Paint glyph) {
  return [box, glyph](Canvas& canvas) {
    Canvas own = canvas.child(box);
    glyph(own);
  };
}

Paint progressBar(Rect box, float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 100.0f) value = 100.0f;
  return [box, value](Canvas& canvas) {
    Canvas own = canvas.child(box);
    const int16_t width = own.width();
    const int16_t height = own.height();
    own.draw(shapes::Rect(0, 0, width, height, Ink));
    const int16_t fill = static_cast<int16_t>((width - 2) * value / 100.0f);
    if (fill > 0) own.draw(shapes::FillRect(1, 1, fill, height - 2, Ink));
  };
}

Paint scrollbar(Rect box, int16_t index, int16_t count, int16_t visible, bool hollow) {
  return [box, index, count, visible, hollow](Canvas& canvas) {
    if (count <= 0) return;
    Canvas own = canvas.child(box);
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
      own.invert(Rect(0, 0, width, top));
      own.invert(Rect(0, static_cast<int16_t>(top + thumb), width,
                      static_cast<int16_t>(height - (top + thumb))));
    } else {
      own.draw(shapes::Rect(0, 0, width, height, Ink));
      own.draw(shapes::FillRect(0, top, width, thumb, Ink));
    }
  };
}

Paint duration(Rect box, DurationFormat format, int32_t seconds, const Font* face, bool known) {
  char out[16];
  durationText(format, seconds, known, out, sizeof(out));
  const std::string formatted(out);
  return [box, formatted, face](Canvas& canvas) {
    Canvas own = canvas.child(box);
    const char* value = formatted.c_str();
    own.draw(shapes::Text(0, baseline(value, face, 1), value, face, 1, Ink));
  };
}

}  // namespace gardener::gfx::ui
