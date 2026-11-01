#pragma once

#include "Font.h"

namespace gardener::gfx {

// u8g2's battery gauge: six levels filled from the bottom and a bolt for the charge. Its u8g2
// entry records only that it was created with Fony 1.4.7, and names no licence. It is the 8 codes
// of 0x30..0x37 of u8g2_font_battery19_tn.c, converted by tools/fonts/from_u8g2.py. Its source
// font: battery19
extern const Font Battery19;

}  // namespace gardener::gfx
