#pragma once

#include <stdint.h>

#include <string>

#include "CaptureDisplay.h"
#include "CapturePicture.h"
#include "Rect.h"

namespace gardener {

// The gesture a page took, drawn in the room a capture's frame leaves outside the glass: the six
// buttons a panel's user can press, across the top of the picture, so a reader sees what was
// pressed and what it drew without the one covering the other. A display knows no gesture by
// itself, so the token to show is the host's to hand over.
//
// Every frame draws all six buttons. A press marks the button it names: one outline for a single
// press, two for a double, and the seconds of a hold written under the glyph it held. The three
// forms are three marks because there are three of them — an outline alone cannot tell a hold from
// a press.
class GestureStrip {
 public:
  // How many captured frames a press stays marked; 1 is the frame it lands on. Four is a second at
  // the capture's default delay, which is long enough for a reader to see.
  static constexpr int Frames = 4;

  // The token the frames that follow mark, for Frames of them. nullptr, an empty token or one that
  // names no button the panel has marks nothing.
  void show(const char* gesture);

  // The component a capture draws the buttons with, in `box` of the frame: as tall as a button and
  // as wide as the room the host gave it. This strip has to outlive the capture it is handed to.
  CaptureDisplay::Component component(gfx::Rect box);

 private:
  // What one button carries while a press on it is marked: the outlines round it, and the seconds
  // of a hold, which is -1 when the press was not one.
  struct Mark {
    int8_t outlines = 0;
    int8_t seconds = -1;
  };

  void draw(gfx::Canvas& canvas);
  void mark(const std::string& key, const std::string& form);

  Mark marks_[CapturePicture::Buttons] = {};
  int frames_ = 0;
};

}  // namespace gardener
