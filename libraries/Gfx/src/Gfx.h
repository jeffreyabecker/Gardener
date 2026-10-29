#pragma once

// Umbrella header for the Gfx drawing library. A build includes this and nothing else: the caller
// owns the frame buffer, a concrete DisplayDevice owns the glass, and a Canvas draws into an extent
// of that buffer. Beside the canvas is the vocabulary a page draws with over it: the drawing
// functions and the layout that divides a box. The scene lifecycle, the gestures a page takes and
// the band across the bottom of the glass are the firmware's panel, in libraries/Gardener.

#include "gfx/BuiltinFont.h"
#include "gfx/Canvas.h"
#include "gfx/DisplayDevice.h"
#include "gfx/Drawing.h"
#include "gfx/Font.h"
#include "gfx/Fonts.h"
#include "gfx/Layout.h"
#include "gfx/Rect.h"
#include "gfx/Shapes.h"
#include "gfx/Text.h"
