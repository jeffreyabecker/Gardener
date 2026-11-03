#pragma once

#include <string>

#include "CaptureDisplay.h"
#include "Rect.h"

namespace gardener {

// A line of text under the glass, in the band a walkthrough's picture keeps for it: what the step
// being drawn is doing, set and cleared by the walk's own lines. The band is the picture's rather
// than the page's, so a note never covers what the panel drew, and the frames are one size from
// the first: a note cleared leaves the band empty rather than the picture shorter.
class NoteBand {
 public:
  // The line every frame from now on carries. Null or empty blanks the band.
  void set(const char* text) { text_ = text != nullptr ? text : ""; }

  // The component a capture draws the note with, in `box` of the frame. This band has to outlive
  // the capture it is handed to.
  CaptureDisplay::Component component(gfx::Rect box);

 private:
  void draw(gfx::Canvas& canvas);

  std::string text_;
};

}  // namespace gardener
