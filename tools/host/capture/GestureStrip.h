#pragma once

#include <stdint.h>

#include "CaptureDisplay.h"
#include "CapturePicture.h"
#include "Rect.h"

namespace gardener {

// The gesture a page took, drawn in the room a capture's frame leaves outside the glass: the six
// buttons a panel's user can press, across the top of the picture, so a reader sees what was pressed
// and what it drew without the one covering the other. A display knows no gesture by itself, so the
// mark is the host's to say: a walkthrough's send line carries the press and the time its mark
// stays, and nothing else touches the buttons.
//
// Every frame draws all six buttons. A mark is what one carries while it is shown: one outline for a
// press, two for a double, and the seconds of a hold written under the glyph it held. The three
// forms are three marks because there are three of them — an outline alone cannot tell a hold from
// a press. The time is the script's, so how long the picture shows a press is the walk's statement
// rather than a count of frames the tool chose.
class GestureStrip {
 public:
  // Marks the buttons one gesture names, each with the form it carries, for `beats` of the session:
  // `ok:single` one outline, `ok:double` two, `ok:7` one outline and seven seconds under the glyph,
  // and `up:single+ok:double` a chord, as the classifier writes one. A mark already on a button is
  // replaced, so a press after a press takes the picture over rather than adding to it. A time of
  // none leaves every button bare.
  void show(const char* gesture, int beats);

  // One beat of the session: a mark loses a beat, and the beat that takes the last one away is the
  // beat it stops being drawn in.
  void age();

  // The component a capture draws the buttons with, in `box` of the frame: as tall as a button and
  // as wide as the room the host gave it. This strip has to outlive the capture it is handed to.
  CaptureDisplay::Component component(gfx::Rect box);

 private:
  // What one button carries while it is marked: the outlines round it, and the seconds of a hold,
  // which is -1 when the mark is not one.
  struct Mark {
    int8_t outlines = 0;
    int8_t seconds = -1;
  };

  void clearAll();
  void draw(gfx::Canvas& canvas);

  Mark marks_[CapturePicture::Buttons] = {};
  // The beats the marks have left; nothing is drawn once they are gone.
  int beats_ = 0;
};

// Whether a gesture names buttons the strip can mark: a button's name with an optional form after a
// colon, and several of them joined with '+', as the classifier writes a chord.
bool isGesture(const char* gesture);

}  // namespace gardener
