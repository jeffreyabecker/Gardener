#include "ui/Duration.h"

#include <stdio.h>

namespace gardener::gfx::ui {
namespace {

// Appends the part a unit contributes, and answers the end of what has been written so far.
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

Duration::Duration(DurationFormat format, const Font* face)
    : Text("", face, 1), format_(format) {
  durationText(format_, seconds_, known_, text_, sizeof(text_));
}

void Duration::setSeconds(int32_t seconds) {
  seconds_ = seconds;
  known_ = true;
  durationText(format_, seconds_, known_, text_, sizeof(text_));
}

void Duration::setUnknown() {
  known_ = false;
  durationText(format_, seconds_, known_, text_, sizeof(text_));
}

void Duration::draw(Canvas& canvas) {
  setValue(text_);
  Text::draw(canvas);
}

}  // namespace gardener::gfx::ui
