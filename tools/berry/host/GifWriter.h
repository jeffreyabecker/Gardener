#pragma once

#include <stdint.h>
#include <vector>

namespace gardener {

// How much a captured GIF is enlarged, by nearest-neighbour: 2 is 256 x 128. Change it and every
// capture scales; the .bmp still is always written at the panel's native size.
constexpr int kRenderScale = 2;

// How long each frame stays on screen, in centiseconds: 100 is one second. Change it to slow the
// whole animation down or speed it up; every capture uses it.
constexpr uint16_t kGifDelayCs = 100;

// A minimal GIF89a writer for the harness's one-bit frames: a two-entry palette (white paper, black
// ink), a NETSCAPE loop so the animation repeats, and one frame per entry, each held for
// kGifDelayCs. Each frame is packed one bit per pixel, row-major and top-down, and is enlarged by
// kRenderScale before it is written.
bool writeGif(const char* path, int16_t width, int16_t height,
              const std::vector<std::vector<uint8_t>>& frames);

}  // namespace gardener
