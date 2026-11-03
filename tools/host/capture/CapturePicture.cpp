#include "CapturePicture.h"

#include "Canvas.h"
#include "layout/Drawing.h"

namespace gardener {

gfx::Rect CapturePicture::bar() {
  return gfx::Rect(static_cast<int16_t>((Width - BarWidth) / 2), 0, BarWidth, ButtonHeight);
}

gfx::Rect CapturePicture::border() {
  return gfx::Rect(0, static_cast<int16_t>(ButtonHeight + Gap), Width, GlassHeight);
}

gfx::Rect CapturePicture::note() {
  return gfx::Rect(0, static_cast<int16_t>(Height - NoteHeight), Width, NoteHeight);
}

gfx::Rect CapturePicture::glass() {
  const gfx::Rect frame = border();
  return gfx::Rect(static_cast<int16_t>(frame.x + Border), static_cast<int16_t>(frame.y + Border),
                   CaptureDisplay::Width, CaptureDisplay::Height);
}

CaptureDisplay::Frame CapturePicture::frame() {
  return CaptureDisplay::Frame{Width, Height, glass()};
}

// One pixel of ink every other pixel along all four sides. The line is dotted rather than drawn so
// it reads as a frame around the picture and not as something a page sent.
CaptureDisplay::Component CapturePicture::borderComponent() {
  return CaptureDisplay::Component{
      border(), gfx::callable([](gfx::Canvas& canvas) {
        const int16_t right = static_cast<int16_t>(canvas.width() - 1);
        const int16_t bottom = static_cast<int16_t>(canvas.height() - 1);
        for (int16_t x = 0; x <= right; x = static_cast<int16_t>(x + 2)) {
          canvas.setPixel(x, 0, gfx::Ink);
          canvas.setPixel(x, bottom, gfx::Ink);
        }
        for (int16_t y = 0; y <= bottom; y = static_cast<int16_t>(y + 2)) {
          canvas.setPixel(0, y, gfx::Ink);
          canvas.setPixel(right, y, gfx::Ink);
        }
      })};
}

}  // namespace gardener
