#pragma once

#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "GifWriter.h"
#include "gfx/DisplayDevice.h"
#include "gfx/Rect.h"
#include "ui/GestureQueue.h"

namespace gardener {

// A panel a case hands a page, a scene or a scene stack: the glass a canvas sends to, the frame it
// draws into, the frames a capture keeps and the gestures the page is handed. It is one object
// because the three are read together — a captured frame is the glass as it stood when a flush
// reported pixels, with the gesture the page had just taken drawn across the top, so a reader of
// the GIF sees what was pressed as well as what was drawn.
//
// A capture keeps every flush that reports pixels and no flush that does not, so a beat a case
// drives is one frame per send. The frames are taken from the glass rather than the frame buffer: a
// rectangle the canvas failed to report keeps the pixels it had, which is what makes a missed
// partial redraw show on the capture. The callout and the overlay hash are drawn into that copy
// only, so nothing a case asserts on the display itself carries them.
class CaptureDisplay : public gfx::DisplayDevice, public gfx::ui::GestureQueue {
 public:
  // The glass the firmware drives, which is the size a recording display has unless it is told
  // otherwise.
  static constexpr int16_t Width = 128;
  static constexpr int16_t Height = 64;

  // How many captured frames the callout of a taken gesture stays on; 1 is the frame it lands on.
  // Raise it to keep the callout readable in a tutorial.
  static constexpr int CalloutFrames = 1;

  // A display at one of the two depths a device may report: 8 is a byte per pixel, the layout the
  // Gfx pages draw in, and 1 is the packed one a one-bit glass keeps. Anything else is 8.
  CaptureDisplay(int16_t width = Width, int16_t height = Height, uint8_t bitsPerPixel = 8);

  // The frame and the glass start clear.
  bool begin() override;

  gfx::Rect size() const override { return gfx::Rect(0, 0, width_, height_); }
  uint8_t bitsPerPixel() const override { return depth_; }
  uint8_t* buffer() override { return frame_.data(); }

  // Sends the rectangle the canvas reports to the glass, and keeps it when a capture is armed. A
  // rectangle holding no pixels sends nothing and keeps nothing, but is still a flush.
  void flush(const gfx::Rect& dirty) override;

  // --- the page's input -----------------------------------------------------
  // The token a page will take. The queue is this display's own, so a case hands the same object
  // to a page as its glass and as its input.
  void press(const char* gesture);

  // The oldest token a page has not taken, or nullptr. The token taken is the gesture state the
  // frames that follow show.
  const char* take() override;

  // Every token pressed, in the order it was pressed, which is what the queue was handed.
  const std::vector<std::string>& gestures() const { return gestures_; }

  // --- what a capture shows -------------------------------------------------
  // The callout without a page seeing it, for a harness whose page takes its presses from
  // somewhere else. An empty token clears the callout.
  void showGesture(const char* gesture);

  // The rows the suite framework hashes because the band belongs to the panel rather than to any
  // page: a capture of a case says which rows it did not draw.
  void markOverlay(int16_t top, int16_t rows);

  // --- the capture ----------------------------------------------------------
  // Starts a sequence at this display's size, discarding anything collected before: every flush
  // that reports pixels is one frame until endCapture().
  void beginCapture();

  // Keeps the glass as one frame now, without a send: a still of what the panel already holds.
  void record();

  // Stops recording and discards what was collected.
  void endCapture();

  // Writes what was collected to `path`, as writeGif does. False when nothing was collected or the
  // file cannot be opened.
  bool save(const char* path) const;

  int frameCount() const;
  // One frame the capture holds, packed one bit per pixel as it will be written. Null when the
  // index is not one of them.
  const std::vector<uint8_t>* frame(int index) const;

  // The frame, the glass, the counters, the collected frames and the gesture state, cleared.
  void clear();

  // --- what the frame holds, for a case to read -----------------------------
  // The pixel at a point as the canvas last drew it, which is what a case asserts on: the byte on a
  // Gray8 frame, 255 or 0 on a Packed1 one. Nothing is read from outside the frame.
  uint8_t at(int16_t x, int16_t y) const;
  bool anyInk(int16_t x, int16_t y, int16_t w, int16_t h) const;
  int inkCount(int16_t x, int16_t y, int16_t w, int16_t h) const;

  int flushCount() const { return flushes_; }
  gfx::Rect lastDirty() const { return lastDirty_; }

  // The glass, packed one bit per pixel, row-major and top-down, as the GIF writer wants it.
  void pack(uint8_t* out) const;

 private:
  // Packs the glass with the callouts and hands it to the capture as one frame.
  void keepFrame();

  int16_t width_;
  int16_t height_;
  uint8_t depth_;
  // The bytes one row of the frame takes: the width when a pixel is a byte, a byte per eight pixels
  // once it is a bit.
  int16_t stride_;
  std::vector<uint8_t> frame_;
  // What the flush has sent, which is the glass a capture keeps.
  std::vector<uint8_t> sent_;
  int flushes_ = 0;
  gfx::Rect lastDirty_{0, 0, 0, 0};

  Capture capture_;
  bool capturing_ = false;

  std::deque<std::string> queue_;
  std::vector<std::string> gestures_;
  // What the last take() answered, which is what the pointer it returned points at.
  std::string taken_;
  std::string callout_;
  int calloutFrames_ = 0;

  int16_t overlayTop_ = -1;
  int16_t overlayRows_ = 0;
};

}  // namespace gardener
