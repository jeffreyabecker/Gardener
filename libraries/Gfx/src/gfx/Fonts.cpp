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
#include "gfx/fonts/FreeMonoBoldOblique12pt7b.h"
#include "gfx/fonts/FreeMonoBoldOblique18pt7b.h"
#include "gfx/fonts/FreeMonoBoldOblique24pt7b.h"
#include "gfx/fonts/FreeMonoBoldOblique9pt7b.h"
#include "gfx/fonts/FreeMonoOblique12pt7b.h"
#include "gfx/fonts/FreeMonoOblique18pt7b.h"
#include "gfx/fonts/FreeMonoOblique24pt7b.h"
#include "gfx/fonts/FreeMonoOblique9pt7b.h"
#include "gfx/fonts/FreeSans12pt7b.h"
#include "gfx/fonts/FreeSans18pt7b.h"
#include "gfx/fonts/FreeSans24pt7b.h"
#include "gfx/fonts/FreeSans9pt7b.h"
#include "gfx/fonts/FreeSansBold12pt7b.h"
#include "gfx/fonts/FreeSansBold18pt7b.h"
#include "gfx/fonts/FreeSansBold24pt7b.h"
#include "gfx/fonts/FreeSansBold9pt7b.h"
#include "gfx/fonts/FreeSansBoldOblique12pt7b.h"
#include "gfx/fonts/FreeSansBoldOblique18pt7b.h"
#include "gfx/fonts/FreeSansBoldOblique24pt7b.h"
#include "gfx/fonts/FreeSansBoldOblique9pt7b.h"
#include "gfx/fonts/FreeSansOblique12pt7b.h"
#include "gfx/fonts/FreeSansOblique18pt7b.h"
#include "gfx/fonts/FreeSansOblique24pt7b.h"
#include "gfx/fonts/FreeSansOblique9pt7b.h"
#include "gfx/fonts/FreeSerif12pt7b.h"
#include "gfx/fonts/FreeSerif18pt7b.h"
#include "gfx/fonts/FreeSerif24pt7b.h"
#include "gfx/fonts/FreeSerif9pt7b.h"
#include "gfx/fonts/FreeSerifBold12pt7b.h"
#include "gfx/fonts/FreeSerifBold18pt7b.h"
#include "gfx/fonts/FreeSerifBold24pt7b.h"
#include "gfx/fonts/FreeSerifBold9pt7b.h"
#include "gfx/fonts/FreeSerifBoldItalic12pt7b.h"
#include "gfx/fonts/FreeSerifBoldItalic18pt7b.h"
#include "gfx/fonts/FreeSerifBoldItalic24pt7b.h"
#include "gfx/fonts/FreeSerifBoldItalic9pt7b.h"
#include "gfx/fonts/FreeSerifItalic12pt7b.h"
#include "gfx/fonts/FreeSerifItalic18pt7b.h"
#include "gfx/fonts/FreeSerifItalic24pt7b.h"
#include "gfx/fonts/FreeSerifItalic9pt7b.h"
#include "gfx/fonts/Picopixel.h"
#include "gfx/fonts/TomThumb.h"
#include "gfx/fonts/UISymbolSans12pt7b.h"
#include "gfx/fonts/UISymbolSans18pt7b.h"
#include "gfx/fonts/UISymbolSans24pt7b.h"

namespace gardener::gfx::fonts {

// --- sans --------------------------------------------------------------------
const Font* const Sans9 = &FreeSans9pt7b;
const Font* const SansBold9 = &FreeSansBold9pt7b;
const Font* const SansOblique9 = &FreeSansOblique9pt7b;
const Font* const SansBoldOblique9 = &FreeSansBoldOblique9pt7b;
const Font* const Sans12 = &FreeSans12pt7b;
const Font* const SansBold12 = &FreeSansBold12pt7b;
const Font* const SansOblique12 = &FreeSansOblique12pt7b;
const Font* const SansBoldOblique12 = &FreeSansBoldOblique12pt7b;
const Font* const Sans18 = &FreeSans18pt7b;
const Font* const SansBold18 = &FreeSansBold18pt7b;
const Font* const SansOblique18 = &FreeSansOblique18pt7b;
const Font* const SansBoldOblique18 = &FreeSansBoldOblique18pt7b;
const Font* const Sans24 = &FreeSans24pt7b;
const Font* const SansBold24 = &FreeSansBold24pt7b;
const Font* const SansOblique24 = &FreeSansOblique24pt7b;
const Font* const SansBoldOblique24 = &FreeSansBoldOblique24pt7b;

// --- mono --------------------------------------------------------------------
const Font* const Mono9 = &FreeMono9pt7b;
const Font* const MonoBold9 = &FreeMonoBold9pt7b;
const Font* const MonoOblique9 = &FreeMonoOblique9pt7b;
const Font* const MonoBoldOblique9 = &FreeMonoBoldOblique9pt7b;
const Font* const Mono12 = &FreeMono12pt7b;
const Font* const MonoBold12 = &FreeMonoBold12pt7b;
const Font* const MonoOblique12 = &FreeMonoOblique12pt7b;
const Font* const MonoBoldOblique12 = &FreeMonoBoldOblique12pt7b;
const Font* const Mono18 = &FreeMono18pt7b;
const Font* const MonoBold18 = &FreeMonoBold18pt7b;
const Font* const MonoOblique18 = &FreeMonoOblique18pt7b;
const Font* const MonoBoldOblique18 = &FreeMonoBoldOblique18pt7b;
const Font* const Mono24 = &FreeMono24pt7b;
const Font* const MonoBold24 = &FreeMonoBold24pt7b;
const Font* const MonoOblique24 = &FreeMonoOblique24pt7b;
const Font* const MonoBoldOblique24 = &FreeMonoBoldOblique24pt7b;

// --- serif -------------------------------------------------------------------
const Font* const Serif9 = &FreeSerif9pt7b;
const Font* const SerifBold9 = &FreeSerifBold9pt7b;
const Font* const SerifItalic9 = &FreeSerifItalic9pt7b;
const Font* const SerifBoldItalic9 = &FreeSerifBoldItalic9pt7b;
const Font* const Serif12 = &FreeSerif12pt7b;
const Font* const SerifBold12 = &FreeSerifBold12pt7b;
const Font* const SerifItalic12 = &FreeSerifItalic12pt7b;
const Font* const SerifBoldItalic12 = &FreeSerifBoldItalic12pt7b;
const Font* const Serif18 = &FreeSerif18pt7b;
const Font* const SerifBold18 = &FreeSerifBold18pt7b;
const Font* const SerifItalic18 = &FreeSerifItalic18pt7b;
const Font* const SerifBoldItalic18 = &FreeSerifBoldItalic18pt7b;
const Font* const Serif24 = &FreeSerif24pt7b;
const Font* const SerifBold24 = &FreeSerifBold24pt7b;
const Font* const SerifItalic24 = &FreeSerifItalic24pt7b;
const Font* const SerifBoldItalic24 = &FreeSerifBoldItalic24pt7b;

// --- tiny and symbols --------------------------------------------------------
const Font* const TomThumbFace = &TomThumb;
const Font* const PicopixelFace = &Picopixel;
const Font* const Symbol12 = &UISymbolSans12pt7b;
const Font* const Symbol18 = &UISymbolSans18pt7b;
const Font* const Symbol24 = &UISymbolSans24pt7b;

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
