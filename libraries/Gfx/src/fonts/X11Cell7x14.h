#pragma once

#include "Font.h"

namespace gardener::gfx {

// X11 misc 7x14: the X11 distribution's public domain fixed font, whose own notice is 'Public
// domain font. Share and enjoy.' It is the 224 codes of 0x20..0xFF of u8g2_font_7x14_tf.c,
// converted by tools/fonts/from_u8g2.py. Its source font: -Misc-Fixed-Medium-R-Normal--
// 14-130-75-75-C-70-ISO10646-1
extern const Font X11Cell7x14;

}  // namespace gardener::gfx
