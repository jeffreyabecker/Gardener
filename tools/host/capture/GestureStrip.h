#pragma once

#include <stdint.h>

#include "CaptureDisplay.h"
#include "CapturePicture.h"
#include "Rect.h"

namespace gardener {

// The gesture a page took, drawn in the room a capture's frame leaves outside the glass: the six
// buttons a panel's user can press, across the top of the picture, so a reader sees what was pressed
// and what it drew without the one covering the other. A display knows no gesture by itself, so the
// marks are the host's to say: a walkthrough's own show and clear lines set them and take them away,
// and nothing else touches them.
//
// Every frame draws all six buttons. A mark is what one carries while it is shown: one outline for a
// press, two for a double, and the seconds of a hold written under the glyph it held. The three
// forms are three marks because there are three of them — an outline alone cannot tell a hold from
// a press. A mark stands until it is cleared, so how long the picture shows a press is the script's
// statement rather than a count of frames the tool chose.
class GestureStrip {
 public:
  // Marks the buttons one gesture names, each with the form it carries: `ok:single` one outline,
  // `ok:double` two, `ok:7` one outline and seven seconds under the glyph, and `up:single+ok:double`
  // a chord, as the classifier writes one. A mark already on a button is replaced, so showing a
  // press again as a hold drops the ring the press left.
  void show(const char* gesture);

  // Takes the mark off one button.
  void clear(const char* button);

  // Takes every mark away.
  void clearAll();

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

  void draw(gfx::Canvas& canvas);

  Mark marks_[CapturePicture::Buttons] = {};
};

// Whether a name is one of the panel's six buttons, which is what a clear names.
bool isButton(const char* name);

// Whether a gesture names buttons the strip can mark: a button's name with an optional form after a
// colon, and several of them joined with '+', as the classifier writes a chord.
bool isGesture(const char* gesture);

}  // namespace gardener
