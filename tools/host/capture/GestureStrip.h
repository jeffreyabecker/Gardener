#pragma once

#include <stdint.h>

#include <string>

#include "CaptureDisplay.h"
#include "Rect.h"

namespace gardener {

// The gesture a page took, drawn in the room a capture's frame leaves outside the glass: the press
// a frame carried is named under the picture rather than across it, so a reader sees what was
// pressed and what it drew without the one covering the other. A display knows no gesture by
// itself, so the token to show is the host's to hand over.
class GestureStrip {
 public:
  // How many captured frames a token stays on; 1 is the frame it lands on. Raise it to keep the
  // label readable in a tutorial.
  static constexpr int Frames = 1;

  // The rows a strip wants outside the glass: one pad, the built-in face's eight rows, one pad.
  static constexpr int16_t Rows = 10;

  // What a gesture token renders as: ok:single -> OK, up:double -> UP×2, cancel:5 -> CANCEL 5s, and
  // a sum joins its parts with " + ". Anything unrecognised comes back as the token uppercased.
  static std::string label(const std::string& gesture);

  // The token the frames that follow show, for Frames of them. nullptr or an empty token shows
  // none.
  void show(const char* gesture);

  // The component a capture draws the strip with, in `box` of the frame: Rows tall, and as wide as
  // the room the host gave it, which is normally under the glass. This strip has to outlive the
  // capture the component is handed to.
  CaptureDisplay::Component component(gfx::Rect box);

 private:
  // Draws the label centred in the box it is handed, then counts one of the frames it stays on.
  void draw(gfx::Canvas& canvas);

  std::string label_;
  int frames_ = 0;
};

}  // namespace gardener
