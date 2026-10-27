#pragma once

// Umbrella header for the Gfx drawing library. A build includes this and nothing else: the caller
// owns the frame buffer, a concrete DisplayDevice owns the glass, and a Canvas draws into an extent
// of that buffer. The components in ui/ are the boxes a page composes over that canvas.

#include "gfx/BuiltinFont.h"
#include "gfx/Canvas.h"
#include "gfx/DisplayDevice.h"
#include "gfx/Font.h"
#include "gfx/Fonts.h"
#include "gfx/Rect.h"
#include "gfx/Shapes.h"
#include "gfx/Text.h"
#include "ui/Component.h"
#include "ui/Duration.h"
#include "ui/Icon.h"
#include "ui/Layout.h"
#include "ui/ProgressBar.h"
#include "ui/Scrollbar.h"
#include "ui/Text.h"
#include "ui/Types.h"
