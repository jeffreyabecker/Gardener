#include "gfx/Fonts.h"

#include <string.h>

#include "gfx/fonts/FreeMono12pt7b.h"
#include "gfx/fonts/FreeMono18pt7b.h"
#include "gfx/fonts/FreeMono24pt7b.h"
#include "gfx/fonts/FreeMono9pt7b.h"
#include "gfx/fonts/FreeMonoBold12pt7b.h"
#include "gfx/fonts/FreeMonoBold18pt7b.h"
#include "gfx/fonts/FreeMonoBold24pt7b.h"
#include "gfx/fonts/FreeMonoBold9pt7b.h"
#include "gfx/fonts/FreeSans12pt7b.h"
#include "gfx/fonts/FreeSans18pt7b.h"
#include "gfx/fonts/FreeSans24pt7b.h"
#include "gfx/fonts/FreeSans9pt7b.h"
#include "gfx/fonts/FreeSansBold12pt7b.h"
#include "gfx/fonts/FreeSansBold18pt7b.h"
#include "gfx/fonts/FreeSansBold24pt7b.h"
#include "gfx/fonts/FreeSansBold9pt7b.h"
#include "gfx/fonts/Picopixel.h"
#include "gfx/fonts/TomThumb.h"

namespace gardener::gfx::fonts {

// --- sans --------------------------------------------------------------------
const Font* const Sans9 = &FreeSans9pt7b;
const Font* const SansBold9 = &FreeSansBold9pt7b;
const Font* const Sans12 = &FreeSans12pt7b;
const Font* const SansBold12 = &FreeSansBold12pt7b;
const Font* const Sans18 = &FreeSans18pt7b;
const Font* const SansBold18 = &FreeSansBold18pt7b;
const Font* const Sans24 = &FreeSans24pt7b;
const Font* const SansBold24 = &FreeSansBold24pt7b;

// --- mono --------------------------------------------------------------------
const Font* const Mono9 = &FreeMono9pt7b;
const Font* const MonoBold9 = &FreeMonoBold9pt7b;
const Font* const Mono12 = &FreeMono12pt7b;
const Font* const MonoBold12 = &FreeMonoBold12pt7b;
const Font* const Mono18 = &FreeMono18pt7b;
const Font* const MonoBold18 = &FreeMonoBold18pt7b;
const Font* const Mono24 = &FreeMono24pt7b;
const Font* const MonoBold24 = &FreeMonoBold24pt7b;

// --- tiny --------------------------------------------------------------------
const Font* const TomThumbFace = &TomThumb;
const Font* const PicopixelFace = &Picopixel;

bool find(const char* name, const Font*& face) {
  if (name == nullptr || name[0] == '\0' || strcmp(name, "builtin") == 0) {
    face = nullptr;
    return true;
  }
  static const struct {
    const char* name;
    const Font* face;
  } Faces[] = {
      {"sans9", Sans9},
      {"sans12", Sans12},
      {"sans18", Sans18},
      {"sans24", Sans24},
      {"sansbold9", SansBold9},
      {"sansbold12", SansBold12},
      {"sansbold18", SansBold18},
      {"sansbold24", SansBold24},
      {"mono9", Mono9},
      {"mono12", Mono12},
      {"mono18", Mono18},
      {"mono24", Mono24},
      {"monobold9", MonoBold9},
      {"monobold12", MonoBold12},
      {"monobold18", MonoBold18},
      {"monobold24", MonoBold24},
      {"tomthumb", TomThumbFace},
      {"picopixel", PicopixelFace},
  };
  for (const auto& entry : Faces) {
    if (strcmp(name, entry.name) == 0) {
      face = entry.face;
      return true;
    }
  }
  return false;
}

}  // namespace gardener::gfx::fonts
