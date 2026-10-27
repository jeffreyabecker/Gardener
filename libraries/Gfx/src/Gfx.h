#pragma once

// Umbrella header for the Gfx drawing library. A build includes this and nothing else: the caller
// owns the frame buffer, a concrete DisplayDevice owns the glass, and a Canvas draws into an extent
// of that buffer. The ui/ folder is the vocabulary a page draws with over that canvas: the drawing
// functions, the layout that divides a box, the gestures a page takes and the scene lifecycle.

#include "gfx/BuiltinFont.h"
#include "gfx/Canvas.h"
#include "gfx/DisplayDevice.h"
#include "gfx/Font.h"
#include "gfx/Fonts.h"
#include "gfx/Rect.h"
#include "gfx/Shapes.h"
#include "gfx/Text.h"
#include "ui/BoolScene.h"
#include "ui/ChoiceScene.h"
#include "ui/Drawing.h"
#include "ui/DurationScene.h"
#include "ui/EditorScene.h"
#include "ui/GestureQueue.h"
#include "ui/Layout.h"
#include "ui/NumberScene.h"
#include "ui/Scene.h"
#include "ui/Ux.h"
