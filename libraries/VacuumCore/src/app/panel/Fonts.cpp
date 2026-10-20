#include "app/panel/Fonts.h"

#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMono24pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/Picopixel.h>
#include <Fonts/TomThumb.h>
#include <string.h>

namespace gardener {
namespace fonts {

const GFXfont* const Sans9 = &FreeSans9pt7b;
const GFXfont* const Sans12 = &FreeSans12pt7b;
const GFXfont* const Sans18 = &FreeSans18pt7b;
const GFXfont* const Sans24 = &FreeSans24pt7b;
const GFXfont* const SansBold9 = &FreeSansBold9pt7b;
const GFXfont* const SansBold12 = &FreeSansBold12pt7b;
const GFXfont* const SansBold18 = &FreeSansBold18pt7b;
const GFXfont* const SansBold24 = &FreeSansBold24pt7b;
const GFXfont* const Mono9 = &FreeMono9pt7b;
const GFXfont* const Mono12 = &FreeMono12pt7b;
const GFXfont* const Mono18 = &FreeMono18pt7b;
const GFXfont* const Mono24 = &FreeMono24pt7b;
const GFXfont* const MonoBold9 = &FreeMonoBold9pt7b;
const GFXfont* const MonoBold12 = &FreeMonoBold12pt7b;
const GFXfont* const MonoBold18 = &FreeMonoBold18pt7b;
const GFXfont* const MonoBold24 = &FreeMonoBold24pt7b;
const GFXfont* const TomThumbFace = &TomThumb;
const GFXfont* const PicopixelFace = &Picopixel;

bool find(const char* name, const GFXfont*& face) {
  if (name == nullptr || strcmp(name, "builtin") == 0) {
    face = nullptr;
    return true;
  }
  static const struct {
    const char* name;
    const GFXfont* face;
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

}  // namespace fonts
}  // namespace gardener
