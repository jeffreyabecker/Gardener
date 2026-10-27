#pragma once

#include <stdint.h>

namespace gardener {

// Masks the overlay band in a packed frame (one bit per pixel, row-major and top-down): clears the
// rows to paper, outlines them, and fills them with a diagonal ink hash. A test capture is of the
// page under test, and the band belongs to the ux component rather than to any page, so the frame
// says which rows a case did not draw.
void drawOverlayHash(uint8_t* packed, int16_t width, int16_t height, int16_t top, int16_t rows);

}  // namespace gardener
