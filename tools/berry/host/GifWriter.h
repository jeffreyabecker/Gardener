#pragma once

#include <stdint.h>
#include <vector>

namespace gardener {

// A minimal GIF89a writer for the harness's one-bit frames: a two-entry palette (white paper, black
// ink), a NETSCAPE loop so the animation repeats, and one frame per entry with the given delay in
// centiseconds. Each frame is packed one bit per pixel, row-major and top-down.
bool writeGif(const char* path, int16_t width, int16_t height,
              const std::vector<std::vector<uint8_t>>& frames, uint16_t delayCs);

}  // namespace gardener
