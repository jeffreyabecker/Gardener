#pragma once

// Umbrella header for the Gfx drawing library. A build includes this and nothing else: the caller
// owns the frame buffer, a concrete DisplayDevice owns the glass, and a View draws into an extent
// of that buffer. Beside the canvas is the vocabulary a page draws with over it: the drawing
// functions and the layout that divides a box. The scene lifecycle, the gestures a page takes and
// the overlay across the bottom of the glass are the firmware's panel, in libraries/Gardener.

#include "BuiltinFont.h"
#include "Canvas.h"
#include "DisplayDevice.h"
#include "Font.h"
#include "Fonts.h"
#include "Rect.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"
#include "shapes/Shapes.h"
#include "shapes/Text.h"
