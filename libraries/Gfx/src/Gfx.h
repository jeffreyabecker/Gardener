#pragma once

// Umbrella header for the Gfx drawing library. A build includes this and nothing else: the caller
// owns the frame buffer, a concrete DisplayDevice owns the glass, and a Canvas draws into an extent
// of that buffer.

#include "gfx/Canvas.h"
#include "gfx/DisplayDevice.h"
#include "gfx/Rect.h"
#include "gfx/Shapes.h"
