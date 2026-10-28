#pragma once

#include <stdint.h>

#include <cstddef>
#include <vector>

namespace gardener {

// How much a captured GIF is enlarged, by nearest-neighbour: 2 is 256 x 128. Change it and every
// capture scales.
constexpr int RenderScale = 2;

// How long each frame stays on screen, in centiseconds: 100 is one second. Change it to slow the
// whole animation down or speed it up; every capture uses it.
constexpr uint16_t GifDelayCs = 100;

// A minimal GIF89a writer for the harness's one-bit frames: a two-entry palette (white paper, black
// ink), a NETSCAPE loop so the animation repeats, and one frame per entry, each held for
// GifDelayCs. Each frame is packed one bit per pixel, row-major and top-down, and is enlarged by
// RenderScale before it is written.
bool writeGif(const char* path, int16_t width, int16_t height,
              const std::vector<std::vector<uint8_t>>& frames);

// The frames a case drives, collected as they are drawn and written when the case is done: a still
// is one frame, an armed capture is one per beat. It knows no panel, page or harness — it takes the
// bytes a display buffer already holds, so a case that advances a fake clock can save what it drove.
class Capture {
 public:
  // How many bytes one packed frame of this size occupies.
  static std::size_t frameBytes(int16_t width, int16_t height);

  // Starts a sequence at this size, discarding anything collected before.
  void arm(int16_t width, int16_t height);

  // Takes a frame packed one bit per pixel, row-major and top-down, copying it. A frame taken
  // before arm() or of the wrong size is ignored.
  void add(const uint8_t* packedFrame);

  // Writes what was collected to `path`, as writeGif does. False when nothing was collected or the
  // file cannot be opened.
  bool save(const char* path) const;

  void disarm();
  int frameCount() const { return static_cast<int>(frames_.size()); }
  int16_t width() const { return width_; }
  int16_t height() const { return height_; }

 private:
  int16_t width_ = 0;
  int16_t height_ = 0;
  std::vector<std::vector<uint8_t>> frames_;
};

}  // namespace gardener
