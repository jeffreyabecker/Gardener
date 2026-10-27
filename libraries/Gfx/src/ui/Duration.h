#pragma once

#include <stddef.h>

#include "gfx/Font.h"
#include "ui/Text.h"

namespace gardener::gfx::ui {

// The largest unit a duration shows.
enum class DurationFormat : uint8_t { Minutes, Seconds, Hours };

// The text a count of seconds prints in a format, written into `out`: `4m30s` for minutes, `30s`
// for seconds, `1h2m3s` for hours, and `--` for a count that is not known. A count of zero or less
// is `0s` in every format.
const char* durationText(DurationFormat format, int32_t seconds, bool known, char* out,
                         size_t capacity);

// Whole seconds formatted in the panel's units of time, drawn as the text it prints. The seconds
// are set before a draw, like a text's value.
class Duration : public Text {
 public:
  explicit Duration(DurationFormat format = DurationFormat::Minutes, const Font* face = nullptr);

  DurationFormat format() const { return format_; }
  void setFormat(DurationFormat format) { format_ = format; }

  int32_t seconds() const { return seconds_; }
  void setSeconds(int32_t seconds);

  // A duration whose count is not known draws `--`, the way a missing reading does.
  bool known() const { return known_; }
  void setUnknown();

  // The text this duration prints for the seconds it was given.
  const char* text() const { return text_; }

  void draw(Canvas& canvas) override;

 private:
  DurationFormat format_;
  int32_t seconds_ = 0;
  bool known_ = true;
  char text_[16] = "";
};

}  // namespace gardener::gfx::ui
